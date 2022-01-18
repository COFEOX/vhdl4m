`timescale 1ns/1ns 

/* Небольшое пояснение по идее проекта. Разумеется 99 из 100 стали бы это делать 
 * на блочной памяти. И от меня наверняка ожидали того же самого. Но во-первых мы не
 * ищем лёгких путей. Хотелось продемонстрировать действительно красивый алгоритм.
 * Я вообще очень люблю сдвиговые регистры, стеки и т.п. Во-вторых в ряде ситуаций
 * такое решение может быть и вполне практичным. Например почти во всех проектах 
 * которые я делал, блочная память была на вес золота. Кроме последнего, там наоборот
 * памяти было залейся, но дико не хватало логики. Так что изыскивать меры по её 
 * экономии безусловно стоит. Тем более сдвиговый регистр штука недорогая. Насколько я
 * помню(спорить сейчас не берусь), у Xilinx одноразрядный сдвиговый регистр длинной
 * до 16, обходится в одну ячейку. Если же говорить о реализации в заказной микросхеме,
 * там стоимость сдвигового регистра и полноценной памяти вообще вещи несопоставимые.
 * 
 * По быстродействию. Скажу сразу. Конвейеризовать я даже не пытался. Не пытался даже 
 * синтезировать. Писалось второпях, скорей-скорей. Ещё кучу времени потратил пока не 
 * сдался, пытаясь заставить работать ghdl(я его всё-таки добью, но потом, в более 
 * спокойной обстановке). 
 * Как бы то ни было, критический путь тут вполне понятен. Это от выхода до входа 
 * сдвигового регистра через функцию R. Там мультиплексор выбора шины, потом
 * сложение, потом мультиплексор выбора сдвига, потом or, потом xor. Набегает конечно
 * не слишком много, но и немало. С конвейеризацией этой операции возможны проблемы.
 * Внеси сюда конвейерную задержку, вся красивая сдвиговая схема рассыплется. Правда 
 * есть мысль. Конвейерная задержка должна быть равна ровно 4. Тогда есть шанс всё 
 * снова собрать. А 4 регистра в такой схеме более чем достаточно. Правда разумеется
 * на практике я этого не проверял. 
 */


module l_shift(  //левый сдвиговый регистр, с задаваемыми размером(size) и шириной шины(width)
	input clk,				//клок
	input enb,				//разрешение сдвига
	input rot,              //циклический сдвиг
	input[width-1:0] si,    //вход
	output[width-1:0] l,	//левый выход
	output[width-1:0] r		//правый выход
	);
	parameter size=4, width=32;

	reg[width-1:0] rr[size-1:0];
	integer i;
	always @(posedge clk)
		if(enb) begin
			if(rot) rr[size-1]<=l;
			else    rr[size-1]<=si;
			for(i=size-1; i>0; i=i-1)
				rr[i-1]<=rr[i];
			end
			
	assign l=rr[0];
	assign r=rr[size-1]; 
endmodule			

module lr_shift(  //двунаправленный сдвиговый регистр, с задаваемыми размером(size) и шириной шины(width)
	input clk,				//клок
	input enb,				//разрешение сдвига
	input dir,				//направление: 0 - влево, 1 - вправо
	input rot,              //циклический сдвиг
	input[width-1:0] si,    //вход
	output[width-1:0] l,	//левый выход
	output[width-1:0] r		//правый выход
	);
	parameter size=4, width=32;

	reg[width-1:0] rr[size-1:0];
	integer i;
	always @(posedge clk)
		if(enb)
			if(dir) begin
				if(rot) rr[0]<=r;
				else    rr[0]<=si;
				for(i=0; i<size-1; i=i+1)
					rr[i+1]<=rr[i]; 
			end
			else begin
				if(rot) rr[size-1]<=l;
				else    rr[size-1]<=si;
				for(i=size-1; i>0; i=i-1)
					rr[i-1]<=rr[i];
			end
		
	assign l=rr[0];
	assign r=rr[size-1];
endmodule			

module rfunc(     //Функция R
	input[31:0] v,
	input[31:0] u1,
	input[31:0] u2,
	input[1:0] s,
	output[31:0] vv
);
	wire[31:0] w=u1+u2;
	wire[31:0] w7=(w<<7)   | (w>>(32-7));
	wire[31:0] w9=(w<<9)   | (w>>(32-9));
	wire[31:0] w13=(w<<13) | (w>>(32-13));
	wire[31:0] w18=(w<<18) | (w>>(32-18));
	reg[31:0] uu;
	
	always @*
		case (s)
			2'b00: uu=w7;
			2'b01: uu=w9;
			2'b10: uu=w13;
			2'b11: uu=w18;
		endcase	
	assign vv=uu^v;
endmodule
	
				
module XorSalsa8( //требуемое устройство
	input clk,			//клок
	input rst,			//сброс
	input bx,			//выбор констант (1) или данных(0)
	input wr,			//запись
	input rd,			//чтение
	input enc,			//кодирование
	input[31:0]  di, 	//входная шина данных
	output[31:0] do,	//выходная шина данных
	output rdy			//готовность кодирования
);


	//Внутренние компоненты
	reg b_enb, bx_enb, b_rot, bx_rot;
	reg[3:0] ss_enb, ss_rot; 
	
	reg[1:0] dir;
	reg[31:0] b_si, ss_si; //для bx si подключен прямо к входной шине, для всех ss_ui входы общие
	wire[31:0] b_l, bx_l, l0, l1, l2, l3;
	reg[1:0] rs;
	reg[31:0] rv, ru1, ru2;
	wire[31:0] rvv; 
	
	l_shift  #(16, 32) rr_b ( //регистр данных
		.clk	(clk),
		.enb	(b_enb),
		.rot	(b_rot),
		.si		(b_si),
		.l		(b_l)
	);
	l_shift  #(16, 32) rr_bx  ( //регистр констант
		.clk	(clk),
		.enb	(bx_enb),
		.rot	(bx_rot),
		.si		(di),
		.l		(bx_l)
	);
	l_shift #(4, 32) ss_u0 ( //операционный регистр u0
		.clk	(clk),
		.enb	(ss_enb[0]),
		.rot	(ss_rot[0]),
		.si		(ss_si),
		.l		(l0)		
	);
	lr_shift #(4, 32) ss_u1 ( //операционный регистр u1
		.clk	(clk),
		.enb	(ss_enb[1]),
		.dir	(dir[0]),
		.rot	(ss_rot[1]),
		.si		(ss_si),
		.l		(l1)		
	);
	l_shift #(4, 32) ss_u2 ( //операционный регистр u2
		.clk	(clk),
		.enb	(ss_enb[2]),
		.rot	(ss_rot[2]),
		.si		(ss_si),
		.l		(l2)		
	);
	lr_shift #(4, 32) ss_u3 ( //операционный регистр u3
		.clk	(clk),
		.enb	(ss_enb[3]),
		.dir	(dir[1]),
		.rot	(ss_rot[3]),
		.si		(ss_si),
		.l		(l3)		
	);
	rfunc rfunc(
		.v	(rv),
		.u1 (ru1),
		.u2	(ru2),
		.s	(rs),
		.vv (rvv)
	);
	
	
	//Режим работы устройства.
	//Устройство работает в двух режимах - ввода/вывода и кодирования
	reg ioenc;    //1 - ввод/вывод, 0 - кодирование
	reg enc_end;  //сигнал окончания кодирования
	always @(posedge clk)
		if(rst|enc_end) ioenc<=1;
		else 
			if(enc) ioenc<=0;

	//------------------- Режим ввода/вывода ----------------------------------//
	
	reg iwr, ird, ienc, wr0, rd0, enc0;  //выделяем сигналы чтения/записи в 1 такт
	always @(posedge clk)
	begin
		rd0<=rd;
		wr0<=wr;
		enc0<=enc;
	end
	always @*
	begin
		if(rd&(!rd0)) ird=ioenc;  else ird=0;
		if(wr&(!wr0)) iwr=ioenc;  else iwr=0;	
		ienc=enc&(!enc0);
	end	
	
	//Счетчик чтения-записи
	reg[3:0] iocnt;
	reg b_rwop;     //операция с регистром данных
	always @(posedge clk)
		if(rst==1) iocnt<=4'h0;
		else
			if(b_rwop)
				iocnt<=iocnt+1; //считает только данные, но не константы
	always @*
		if(ioenc&(!bx)) b_rwop=iwr|ird; //чтение или запись в режиме io при низком bx
		else b_rwop=0;
		
	//Сигналы режима ввода-вывода
	reg[3:0] io_enb, io_rot;
	reg[31:0] ss_o; //выход регистров ss_u, участвующий в формировании выходной шины
	reg[31:0] io_si;
	reg[31:0] d_out; //выход
	always @*
	begin
		b_si=di ^ bx_l;       //это пишется в регистр данных и операционные регистры
		io_si=b_si;
		bx_enb=iwr|ird;         //bx сдвигается при любых операциях чтения-записи
		bx_rot=(!bx)|rd;        //и циклится при операциях с b и любых чтениях
		b_enb=(iwr|ird)&(!bx);  //b сдвигается при чтении и записи если bx низкий
		b_rot=bx|rd|(!wr);      //b циклится при чтении, операциях с bx и при низком wr
		io_rot={4{b_rot}};
		
		case(iocnt)
			4'd0, 4'd5, 4'd10, 4'd15: begin
				if(b_enb) io_enb=4'b0001; else io_enb=4'd0;
				ss_o=l0;
			end
			4'd1, 4'd6, 4'd11, 4'd12: begin
				if(b_enb) io_enb=4'b0010; else io_enb=4'd0;
				ss_o=l1;
			end
			4'd2, 4'd7, 4'd8,  4'd13: begin
				if(b_enb) io_enb=4'b0100; else io_enb=4'd0;
				ss_o=l2;
			end
			4'd3, 4'd4, 4'd9,  4'd14: begin
				if(b_enb) io_enb=4'b1000; else io_enb=4'd0;
				ss_o=l3;
			end
		endcase
		if(bx) d_out=bx_l; else d_out=ss_o+b_l;
		//if(rd) ss_rot=4'd15; else ss_rot=4'd0;
	end
	
	//----------------------------- Режим кодирования ---------------------------//
	reg[1:0] encnt4;    //счетчик циклов
	reg[4:0] encnt34;   //счётчик на 34 для внутреннего цикла
	reg enc34_1;        //дополнительный бит счётчика для формирования паузы
	reg pause;          //пауза
	reg enc_eoc;		//конец цикла
	//введение паузы непосредственно в счетчик сильно облегчает управление устройством.
	always @(posedge clk)
		if(rst) encnt34<=5'd31;
		else
		if(ienc) encnt34<=5'd0;
		else
		if(!ioenc)
			if(encnt34[3:0]==4'd0) encnt34<=encnt34+enc34_1;
			else
			if(!enc_end)
				encnt34<=encnt34+1;
		
	always @(posedge clk) 
		if(ienc|rst) encnt4<=2'd0;
		else
		if(enc_eoc) encnt4=encnt4+1;
	
	always @(posedge clk) //тут формируется пауза в 1 такт в начале и середине цикла
		if(ienc|encnt34[2]|rst) enc34_1<=0;
		else
		if(encnt34[3:0]==4'd0) enc34_1<=1;
		
	always @* begin
		if((enc34_1==0)&&(encnt34[3:0]==0)) pause=1; else pause=0;
		if(encnt34==5'd31) enc_eoc=1; else enc_eoc=0;
		if((encnt34==5'd31)&&(encnt4==2'd3)) enc_end=1; else enc_end=0;
	end
	
	//сигналы управления сдвиговыми регистрами
	reg[3:0] enc_enb, enc_enb1, enc_enb2;
	reg[3:0] enc_rot, enc_rot1, enc_rot2;
	reg[1:0] enc_dir; 
	
	//регулярные сигналы и шины устройства R
	always @*
	begin
		rs=encnt34[3:2];
		case (encnt34[4:2])
			3'd0: begin //R(3, 0, 1);
				rv=l3; ru1=l0; ru2=l1;
				enc_enb1=4'b1011; enc_rot1=4'b0111;
			end
			3'd1: begin //R(2, 3, 0);
				rv=l2; ru1=l3; ru2=l0;
				enc_enb1=4'b1101; enc_rot1=4'b1011;
			end
			3'd2: begin //R(1, 2, 3);
				rv=l1; ru1=l2; ru2=l3;
				enc_enb1=4'b1110; enc_rot1=4'b1101;
			end
			3'd3: begin	//R(0, 1, 2);
				rv=l0; ru1=l1; ru2=l2;
				enc_enb1=4'b0111; enc_rot1=4'b1110;
			end
			3'd4: begin	//R(1, 0, 3);
				rv=l1; ru1=l0; ru2=l3;
				enc_enb1=4'b1011; enc_rot1=4'b1101;
			end
			3'd5: begin //R(2, 1, 0);
				rv=l2; ru1=l1; ru2=l0;
				enc_enb1=4'b0111; enc_rot1=4'b1011;
			end
			3'd6: begin	//R(3, 2, 1);
				rv=l3; ru1=l2; ru2=l1;
				enc_enb1=4'b1110; enc_rot1=4'b0111;
			end
			3'd7: begin	//R(0, 3, 2);
				rv=l0; ru1=l3; ru2=l2;
				enc_enb1=4'b1101; enc_rot1=4'b1110;
			end
		
		endcase
	end
	
	//Сигналы режима кодирования
	always @* begin
		if(encnt34[3:0]==0) begin
			enc_enb2=enc_enb1 | 4'b0100;  //сразу после паузы ещё на один такт сдвигается u2 
			enc_rot2=enc_rot1 | 4'b0100;
		end
		else begin
			enc_enb2=enc_enb1; 
			enc_rot2=enc_rot1;
		end
		if(pause) begin
			enc_dir[0]=~encnt34[4];
			enc_dir[1]=encnt34[4];
			enc_enb=4'b1110;
			enc_rot=4'b1110;
		end
		else begin
			enc_dir=2'b00;
			enc_enb=enc_enb2;
			enc_rot=enc_rot2;
		end
	end
				
	//Мультиплексор сигналов, в зависимости от режима
	always @*
		if(ioenc) begin //режим ввода-вывода
			ss_si=io_si;
			ss_enb=io_enb;
			ss_rot=io_rot;
			dir=2'b0;
		end
		else begin
			ss_si=rvv;
			ss_enb=enc_enb;
			ss_rot=enc_rot;
			dir=enc_dir;
		end
			
	
	assign rdy=ioenc;
	assign do=d_out;
			
		

endmodule				
				 
			