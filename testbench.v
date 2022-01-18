`timescale 1ns/100ps 
module testbench;
	reg clk;   //общий клок
	always 
		#100 clk=~clk;

	reg rst, bx, wr, rd, enc;
	reg[31:0] di;
	wire rdy;
	wire[31:0] do;
	
	XorSalsa8 test(
		.clk	(clk),
		.rst	(rst),
		.bx		(bx),
		.wr		(wr),
		.rd		(rd),
		.enc	(enc),
		.di		(di),
		.do		(do),
		.rdy	(rdy)
	);
	
	reg[31:0] mem_b   [0:15];
	reg[31:0] mem_bx  [0:15];
	reg[31:0] mem_test [0:15];
	
	
	task loadBx;
		integer i;
		begin
			bx=1;
			rd=0;
			for(i=0; i<16; i++) begin
				di=mem_bx[i];
				wr=1;
				@(posedge clk);
				@(posedge clk);
				wr=0;	
				@(posedge clk);
				@(posedge clk);
			end
		end
	endtask
	
	task loadB;
		integer i;
		begin
			bx=0;
			rd=0;
			for(i=0; i<16; i++) begin
				di=mem_b[i];
				wr=1;
				@(posedge clk);
				@(posedge clk);
				wr=0;	
				@(posedge clk);
				@(posedge clk);
			end
		end
	endtask
	
	task readBlock;
		integer i;
		begin
			wr=0;
			for(i=0; i<16; i++) begin
				rd=1;
				@(posedge clk);
				@(posedge clk);
				rd=0;
				@(posedge clk);
				@(posedge clk);
			end
		end
	endtask
	
	reg[31:0] result;
	reg[31:0] verify;
	reg[31:0] error;
	reg[3:0] number;
	task testBlock;
		integer i;
		begin
			wr=0;
			bx=0;
			verify<=mem_test[0];
			@(posedge clk);
			@(posedge clk);
			for(i=1; i<16; i++) begin
				
				error<=verify^do;
				number<=i;
				rd=1;
				@(posedge clk);
				verify<=mem_test[i];
				@(posedge clk);
				rd=0;
				@(posedge clk);
				@(posedge clk);
			end
		end
	endtask
			
		
		

	task skipClk(integer n); 
	integer i;
	begin
		for(i=0; i<n-1; i=i+1)
			@(posedge clk);
	end
	endtask
	
	initial begin
		$dumpfile("xorsalsa8.vcd");
		$dumpvars(0, testbench);
		$readmemh("./bx.txt", mem_bx);
		$readmemh("./b.txt",  mem_b);
		$readmemh("./out.txt", mem_test);
		clk=0;
		rst=1;
		bx=1;
		rd=0;
		wr=0; 
		enc=0;
		verify=0;
		error=0;
		number=0;
		@(posedge clk);
		@(posedge clk);
		rst=0;
		loadBx;
		
		@(posedge clk);
		@(posedge clk);
		loadB;
		
		@(posedge clk);
		@(posedge clk);
		
		readBlock;
		readBlock;
		
		enc=1;
		@(posedge clk);
		@(posedge clk);
		enc=0;

		@(posedge clk);
		@(posedge clk);
		@(rdy)
		skipClk(10);
		
		testBlock;
		
		
		$finish;	
		
	end	
	

endmodule