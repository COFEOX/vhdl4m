/*
 * Всю творческую работу сделаем на С++.
 * В результате получим некий класс, который легко
 * и просто можно будет перенести на железо.
 */

#include <stdio.h>
#include <stdlib.h>

/*
 * Прежде всего скопируем и удобно отформатируем саму целевую функцию.
 * Это наша Мекка. Будем молиться, обратясь к ней лицом :))
 */
typedef unsigned int uint32_t;
void xor_salsa8(uint32_t B[16], const uint32_t Bx[16])
{
	uint32_t x00,x01,x02,x03,x04,x05,x06,x07,x08,x09,x10,x11,x12,x13,x14,x15;
	int i;

	x00 = (B[ 0] ^= Bx[ 0]);
	x01 = (B[ 1] ^= Bx[ 1]);
	x02 = (B[ 2] ^= Bx[ 2]);
	x03 = (B[ 3] ^= Bx[ 3]);
	x04 = (B[ 4] ^= Bx[ 4]);
	x05 = (B[ 5] ^= Bx[ 5]);
	x06 = (B[ 6] ^= Bx[ 6]);
	x07 = (B[ 7] ^= Bx[ 7]);
	x08 = (B[ 8] ^= Bx[ 8]);
	x09 = (B[ 9] ^= Bx[ 9]);
	x10 = (B[10] ^= Bx[10]);
	x11 = (B[11] ^= Bx[11]);
	x12 = (B[12] ^= Bx[12]);
	x13 = (B[13] ^= Bx[13]);
	x14 = (B[14] ^= Bx[14]);
	x15 = (B[15] ^= Bx[15]);
	for (i = 0; i < 8; i += 2)
	{
		#define R(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
		// Operate on columns.
		x04 ^= R(x00+x12, 7);  //u3=R(u0, u1, 7)
		x09 ^= R(x05+x01, 7);
		x14 ^= R(x10+x06, 7);
		x03 ^= R(x15+x11, 7);

		x08 ^= R(x04+x00, 9); //u2=R(u3, u0, 9)
		x13 ^= R(x09+x05, 9);
		x02 ^= R(x14+x10, 9);
		x07 ^= R(x03+x15, 9);

		x12 ^= R(x08+x04,13); //u1=R(u2, u3, 13)
		x01 ^= R(x13+x09,13);
		x06 ^= R(x02+x14,13);
		x11 ^= R(x07+x03,13);

		x00 ^= R(x12+x08,18); //u0=R(u1, u2, 18)
		x05 ^= R(x01+x13,18);
		x10 ^= R(x06+x02,18);
		x15 ^= R(x11+x07,18);

		// Operate on rows.
		x01 ^= R(x00+x03, 7); //v1=R(v0, v3, 7)
		x06 ^= R(x05+x04, 7);
		x11 ^= R(x10+x09, 7);
		x12 ^= R(x15+x14, 7);

		x02 ^= R(x01+x00, 9); //v2=R(v1, v0, 9)
		x07 ^= R(x06+x05, 9);
		x08 ^= R(x11+x10, 9);
		x13 ^= R(x12+x15, 9);

		x03 ^= R(x02+x01,13); //v3=R(v2, v1, 13)
		x04 ^= R(x07+x06,13);
		x09 ^= R(x08+x11,13);
		x14 ^= R(x13+x12,13);

		x00 ^= R(x03+x02,18); //v0=R(v3, v2, 18)
		x05 ^= R(x04+x07,18);
		x10 ^= R(x09+x08,18);
		x15 ^= R(x14+x13,18);

		#undef R
	}
	B[ 0] += x00;
	B[ 1] += x01;
	B[ 2] += x02;
	B[ 3] += x03;
	B[ 4] += x04;
	B[ 5] += x05;
	B[ 6] += x06;
	B[ 7] += x07;
	B[ 8] += x08;
	B[ 9] += x09;
	B[10] += x10;
	B[11] += x11;
	B[12] += x12;
	B[13] += x13;
	B[14] += x14;
	B[15] += x15;
}
/*
 * Обратившись лицом к Мекке, замечаем, что переменные х (с различными номерами),
 * участвуют в операциях четверками. В первой и второй частях цикла это соответственно:
 * u0={0, 5, 10, 15}, u1={12, 1, 6, 11}, u2={8, 13, 2, 7}, u3={4, 9, 14, 3}
 * v0={0, 5, 10, 15}, v1={1, 6, 11, 12}, v2={2, 7, 8, 13}, v3={3, 4, 9, 14}
 * Четвёрки я занумеровал по наименьшему номеру x в неё входящему.
 * Видно, что четвёрки u0 и v0 совпадают. u1,u2,u3 отличаются от v1,v2,v3
 * соответственно на 1, 2 и 3 циклических сдвига влево.
 * Всё это наводит на мысли о красивой, быстрой и экономной реализации на сдвиговых регистрах.
 * Это же рассмотрение приводит к ограничению возможного распарраллеливания операций.
 * Одновременно может быть выполнено не более 4 операций R - с тремя четвёрками.
 * Дальнейшие операции начинают зависеть от ранее полученных данных.
 */

class ShiftReg
{
	/*
	 * Это наш сдвиговый регистр. Он умеет только сдвигать влево(lshift) и вправо(rshift).
	 * Все операции выполняются за 1 такт.
	 * Каждый момент времени доступны только два значения:
	 * крайние левое l и правое r
	 * Остальные переменные скрыты внутри.
	 */
public:
	int size;
	uint32_t *buf;
	uint32_t l;
	uint32_t r;
	ShiftReg(int size)
	{
		this->size=size;
		buf=(uint32_t*)malloc(sizeof(uint32_t)*size);
		l=buf[0];
		r=buf[size-1];
	}
	~ShiftReg()
	{
		free(buf);
	}
	void lshift(uint32_t v)
	{
		for(int i=1; i<size; i++) buf[i-1]=buf[i];
		buf[size-1]=v;
		l=buf[0];
		r=buf[size-1];
	}
	void rshift(uint32_t v)
	{
		for(int i=size-1; i>0; i--) buf[i]=buf[i-1];
		buf[0]=v;
		l=buf[0];
		r=buf[size-1];
	}
};

class XorSalsa8_1
{
	/*
	 * Это класс-основной компанент устройства, реализующего наш алгоритм.
	 * Устройство имеет сдвиговые регистры констант - Bx, данных - B
	 * и четыре операционных сдвиговых регистра - u[0],u[1],u[2] и u[3]
	 * Оно умеет устанавливать константы и данные (setBx и setB соответственно),
	 * последовательно выдавать закодированные данные (read), а так же делать базовый
	 * шаг алгоритма (step). Все эти операции выполняются за 1 такт.
	 * Кроме того за 1 такт устройство умеет сдвигать любые операционные регистры (lshift, rshift)
	 */
public:
	ShiftReg *B, *Bx;
	int count;
	ShiftReg *u[4];
	XorSalsa8_1()
	{
		count=0;
		B=new ShiftReg(16);
		Bx=new ShiftReg(16);
		for(int i=0; i<4; i++) u[i]=new ShiftReg(4);
	}
	~XorSalsa8_1()
	{
		delete B;
		delete Bx;
		for(int i=0; i<4; i++) delete(u[i]);
	}
	static uint32_t R(uint32_t a, int b)
	{
		uint32_t w1= a << b;
		uint32_t w2=a >> (32 - b);
		return w1|w2;
	}

	void reset() {count=0;}
	void setBx(uint32_t bx) {Bx->lshift(bx);}
	void lshift(int n) {u[n]->lshift(u[n]->l);}
	void rshift(int n) {u[n]->rshift(u[n]->r);}
	void setB(uint32_t b)
	{
		uint32_t v=Bx->l^b;
		int r=0;
		switch(count)
		{
		case 0: case 5: case 10: case 15: r=0; break;
		case 1: case 6: case 11: case 12: r=1; break;
		case 2: case 7: case 8 : case 13: r=2; break;
		case 3: case 4: case 9 : case 14: r=3; break;
		}
		count=(count+1)&15;
		u[r]->lshift(v);
		B->lshift(v);
		Bx->lshift(Bx->l);
	}
	void step(int d, int s1, int s2, int sft)
	{
		u[d]->lshift(u[d]->l^R(u[s1]->l+u[s2]->l, sft));
		lshift(s1);
		lshift(s2);
	}
	uint32_t read()
	{
		int r=0;
		switch(count)
		{
		case 0: case 5: case 10: case 15: r=0; break;
		case 1: case 6: case 11: case 12: r=1; break;
		case 2: case 7: case 8 : case 13: r=2; break;
		case 3: case 4: case 9 : case 14: r=3; break;
		}
		count=(count+1)&15;
		uint32_t v=B->l+u[r]->l;
		B->lshift(B->l);
		u[r]->lshift(u[r]->l);
		return v;
	}

};

/*
 * Проверим, как выполняются операции setB и read
 * Забьем регистр констант нулями (чтоб под ногами не путались),
 * а в регистр данных запишем числа от 0 до 15.
 * Если всё верно, в операционных регистрах мы увидим четверки v0,v1,v2,v3(см. выше),
 * а при чтении получим удвоенные значения исходных чисел.
 */
void test1()
{
	uint32_t v[4][4]={
		{0, 5, 10, 15},
		{1, 6, 11, 12},
		{2, 7, 8, 13},
		{3, 4, 9, 14}
	};
	XorSalsa8_1 *xr=new XorSalsa8_1();
	for(int i=0; i<16; i++) xr->setBx(0);
	xr->reset();
	for(int i=0; i<16; i++) xr->setB(i);
	int err=0;
	printf("------- setB test -------\n");
	for(int i=0; i<4; i++)
	{
		uint32_t *buf=xr->u[i]->buf;
		for(int j=0; j<4; j++)
		{
			uint32_t w=buf[j];
			printf("%d ",w);
			if(w!=v[i][j])
				err=1;
		}
		printf("\n");
	}
	if(!err)
		printf("setB test passed\n");

	else
		printf("setB fails\n");

	err=0;
	printf("------- read test -------\n");
	for(unsigned int i=0; i<16; i++)
	{
		uint32_t w=xr->read();
		printf("%d ", w);
		if(w!=2*i)
			err=1;
	}
	printf("\n");
	if(!err)
		printf("read test passed\n");
	else
		printf("read fails\n");
	delete xr;
}

/*
 * После загрузки данных мы получаем в операционных регистрах четвёрки v0,v1,v2 и v3.
 * Для того чтобы войти в цикл, нам нужны сдвинутые четверки - u0,u1,u2 и u3.
 * Давайте проверим, как наше устройство умеет сдвигать операционные регистры.
 * Заодно проверим базовый шаг алгоритма.
 * Для этого в нашей функции xor_salsa8 уберём  цикл и оставим только операции
 * с первой четвёркой. А затем выполним ещё один тест
 */
void xor_2(uint32_t B[16], const uint32_t Bx[16])
{
	uint32_t x00,x01,x02,x03,x04,x05,x06,x07,x08,x09,x10,x11,x12,x13,x14,x15;


	x00 = (B[ 0] ^= Bx[ 0]);
	x01 = (B[ 1] ^= Bx[ 1]);
	x02 = (B[ 2] ^= Bx[ 2]);
	x03 = (B[ 3] ^= Bx[ 3]);
	x04 = (B[ 4] ^= Bx[ 4]);
	x05 = (B[ 5] ^= Bx[ 5]);
	x06 = (B[ 6] ^= Bx[ 6]);
	x07 = (B[ 7] ^= Bx[ 7]);
	x08 = (B[ 8] ^= Bx[ 8]);
	x09 = (B[ 9] ^= Bx[ 9]);
	x10 = (B[10] ^= Bx[10]);
	x11 = (B[11] ^= Bx[11]);
	x12 = (B[12] ^= Bx[12]);
	x13 = (B[13] ^= Bx[13]);
	x14 = (B[14] ^= Bx[14]);
	x15 = (B[15] ^= Bx[15]);
	//for (i = 0; i < 8; i += 2)
	{
		#define R(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
		// Operate on columns.
		x04 ^= R(x00+x12, 7);  //u3=R(u0, u1, 7)
		x09 ^= R(x05+x01, 7);
		x14 ^= R(x10+x06, 7);
		x03 ^= R(x15+x11, 7);

		#undef R
	}
	B[ 0] += x00;
	B[ 1] += x01;
	B[ 2] += x02;
	B[ 3] += x03;
	B[ 4] += x04;
	B[ 5] += x05;
	B[ 6] += x06;
	B[ 7] += x07;
	B[ 8] += x08;
	B[ 9] += x09;
	B[10] += x10;
	B[11] += x11;
	B[12] += x12;
	B[13] += x13;
	B[14] += x14;
	B[15] += x15;
}

void test2()
{
	uint32_t u[4][4]={
		{0, 5, 10, 15},
		{12, 1, 6, 11},
		{8, 13, 2, 7},
		{4, 9, 14, 3}
	};
	uint32_t b[16], bx[16];
	for(int i=0; i<16; i++) {b[i]=i; bx[i]=0;}


	XorSalsa8_1 *xr=new XorSalsa8_1();
	for(int i=0; i<16; i++) xr->setBx(bx[i]);
	xr->reset();
	for(int i=0; i<16; i++) xr->setB(b[i]);
	int err=0;
	printf("------- shift test -------\n");

	xr->rshift(1); xr->lshift(2); xr->lshift(3);  //это один такт
	xr->lshift(2);                                //это ещё один такт
	for(int i=0; i<4; i++)
	{
		uint32_t *buf=xr->u[i]->buf;
		for(int j=0; j<4; j++)
		{
			uint32_t w=buf[j];
			printf("%d ",w);
			if(w!=u[i][j])
				err=1;
		}
		printf("\n");
	}
	if(!err)
		printf("shift test passed\n");
	else
		printf("shift fails\n");

	printf("------- step test -------\n");
	err=0;
	xor_2(b, bx);

	xr->step(3, 0, 1, 7);
	xr->step(3, 0, 1, 7);
	xr->step(3, 0, 1, 7);
	xr->step(3, 0, 1, 7);

	//Поскольку мы сдвигали, перед проверкой возвратим всё на место
	xr->lshift(1); xr->lshift(2); xr->rshift(3);  //это один такт
	xr->lshift(2);                                //это ещё один такт

	for(int i=0; i<16; i++)
	{
		uint32_t w=xr->read();
		printf("%d %d\n", w, b[i]);
		if(w!=b[i]) err=1;
	}
	if(!err)
		printf("step test passed\n");
	else
		printf("step fails\n");
	delete xr;
}

/*
 * Теперь когда мы убедились что компонент нашего проектируемого устройства
 * работает правильно, можно реализовать и само устройство в законченном виде.
 * Оно умеет устанавливать константы и данные (setBx, setB),
 * кодировать полученный блок данных (encode),
 * и выдавать последовательно полученный результат (read)
 */
class XorSalsa8
{
public:
	XorSalsa8_1 *xr;
	int count;
	int count4;
	XorSalsa8()
	{
		xr=new XorSalsa8_1();
		count=0;
		count4=0;
	}
	~XorSalsa8() {delete xr;}
	void setB(uint32_t b) {xr->setB(b);}
	void setBx(uint32_t bx) {xr->setBx(bx);}
	uint32_t read() {return xr->read();}
	void reset()
	{
		count=0;
		count4=0;
		xr->reset();
	}
	void show()
	{
		for(int i=0; i<4; i++)
		{
			uint32_t *buf=xr->u[i]->buf;
			printf("%d: ", i);
			for(int j=0; j<4; j++)
			{
				uint32_t w=buf[j];
				printf("%d ",w);

			}
			printf("\n");
		}
		printf("\n");
	}
	void encode_once()
	{//тут для ясности всё расписано по тактам
		for(count=0; count<34; count++)
		{
			switch(count)
			{
			//u1 и u3 должны быть готовы, поэтому приходится пропустить такт
			case 0:  xr->rshift(1); xr->lshift(2);  xr->lshift(3); break; //переход от v к u

			//u2 сейчас в операциях не участвует, поэтому её можно подготовить во время вычислений
			case 1:  xr->step(3, 0, 1, 7); xr->lshift(2);break;          //u3=R(u0, u1, 7)+продолжение перехода

			case 2: case 3: case 4:
				xr->step(3, 0, 1, 7); break;                              //u3=R(u0, u1, 7)

			case 5: case 6: case 7: case 8:
				xr->step(2, 3, 0, 9); break;                              //u2=R(u3, u0, 9)

			case 9: case 10: case 11: case 12:
				xr->step(1, 2, 3, 13); break;                             //u1=R(u2, u3, 13)

			case 13: case 14: case 15:
				xr->step(0, 1, 2, 18);  break;                             //u0=R(u1, u2, 18)

			//сейчас в вычислениях не участвует только u3, и только её можно переводить в v3
			case 16:
				xr->step(0, 1, 2, 18); xr->rshift(3);  break;             //Начало перехода u->v

			//v1 не готова, поэтому приходится пропустить такт
			case 17:
				xr->lshift(1); xr->lshift(2); break;

			case 18:
				xr->step(1, 0, 3, 7); xr->lshift(2); break;              //v1=R(v0, v3, 7)

			case 19: case 20: case 21:
				xr->step(1, 0, 3, 7); break;

			case 22: case 23: case 24: case 25:
				xr->step(2, 1, 0, 9); break;                            //v2=R(v1, v0, 9)

			case 26: case 27: case 28: case 29:
				xr->step(3, 2, 1, 13); break;                            //v3=R(v2, v1, 13)

			case 30: case 31: case 32: case 33:
				xr->step(0, 3, 2, 18); break;                           //v0=R(v3, v2, 18)
			}
		}
	}
	/*
	 * Заметим так же, что регистры u[1] и u[3] сдвигаются и влево и вправо,
	 * а u[0] и u[2] - только влево.
	 * Поскольку двунаправленный регистр обходится дороже однонаправленного,
	 * мы можем немного сэкономить, сделав регистры u[0] и u[2] однонаправленными.
	 * Так же разумеется однонаправленными будут B и Bx
	 */
	void encode()
	{
		for(count4=0; count4<4; count4++)
			encode_once();
	}

};

void test3()
{
	uint32_t b[16], bx[16];
	for(int i=0; i<16; i++) {b[i]=123+i*i; bx[i]=456+i*i*i;}

	//Записываем тесты для vhdl
	FILE *fb=fopen("b.txt", "wt");
	FILE *fbx=fopen("bx.txt", "wt");
	FILE *fout=fopen("out.txt", "wt");
	for(int i=0; i<16; i++)
	{
		fprintf(fb, "%08X\n", b[i]);
		fprintf(fbx, "%08X\n", bx[i]);
	}
	fclose(fb);
	fclose(fbx);



	XorSalsa8 *xr=new XorSalsa8();
	for(int i=0; i<16; i++) xr->setBx(bx[i]);
	xr->reset();
	for(int i=0; i<16; i++) xr->setB(b[i]);
	int err=0;
	printf("------- encode test -------\n");
	err=0;
	xor_salsa8(b, bx);

	xr->encode();

	for(int i=0; i<16; i++)
	{
		uint32_t w=xr->read();
		printf("0x%08x 0x%08x\n", w, b[i]);
		if(w!=b[i]) err=1;
		fprintf(fout, "%08X\n", b[i]);
	}
	fclose(fout);
	if(!err)
		printf("encode test passed\n");
	else
		printf("encode  fails\n");
	delete xr;
}

/*
 * На этом всё. Мы имеем прототип устройства на C++, вполне пригодный для портирования на железо.
 *
 */

int main()
{//Запускаем все тесты
	/*test1();
	printf("\n");
	test2();
	printf("\n");
	*/
	test3();


	//printf("0x%08x\n", XorSalsa8_1::R(0x12345678, 7));
	//printf("0x%08x\n", XorSalsa8_1::R(0x87654321, 7));
	return 0;
}
