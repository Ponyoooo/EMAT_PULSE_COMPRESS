`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2020/10/06 16:17:46
// Design Name: 
// Module Name: SampleCodeUsingBRAM
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module SampleCodeUsingBRAM(
    input clka,//�˿�Aʱ��65MHz
    input clkb,//�˿�Bʱ��100MHz
	input rst,//��λ�ź�
	
    input[11:0] AD_Data_in,//AD����
	input[11:0] TR_Data_in,//TR����
    input wr_start_first,//��һ��д�ź�
	input wr_start_second,//�ڶ���д�ź�
    input burst_delay_flag,//AXI Burst֮����ʱ�źţ���Ϊ��ʹ���ź�
    
    output wire [31:0] Data_out,//���
	output reg half_full_flag = 'b0,//�����ź�
    output reg rd_en = 'b0//ȫ���ź�
    );
	
	reg [1:0] CLKA_DIV_COUNTER = 'b0;
	reg CLKA_DIV = 'b0;
	always@(posedge clka)
	begin
		if(CLKA_DIV_COUNTER==2'b11)
		begin
			CLKA_DIV_COUNTER<='b0;
			CLKA_DIV<=~CLKA_DIV;
		end
		else
		begin
			CLKA_DIV_COUNTER<=CLKA_DIV_COUNTER+'b1;
			CLKA_DIV<=CLKA_DIV;
		end
	end
	
	reg full_flag = 'b0;
	
	//12λAD������չΪ32λ����
	wire [31:0] Data_in;
	reg  [15:0] AD_Data_in_16 = 'b0;
	reg  [15:0] TR_Data_in_16 = 'b0;
	always@(posedge clka)
	begin
		AD_Data_in_16[11:0] <= AD_Data_in;
		TR_Data_in_16[11:0] <= TR_Data_in;
	end
	assign Data_in[15:0] = AD_Data_in_16;
	assign Data_in[31:16]= TR_Data_in_16;
    
    reg PORTA_WEA = 1;//A�˿�д��
    reg PORTB_WEB = 0;//B�˿ڶ���
    
    reg PORTA_EN = 0;//дʹ��
	
	reg [12:0] addra=0;//PortA��ַ(д��ַ)
	reg [12:0] addrb=0;//PortB��ַ(����ַ)
	
	//������һ��дʹ��������ڶ���дʹ������
	reg init_wr_ff=0;
	reg init_wr_ff2=0;
	reg init_wr2_ff=0;
	reg init_wr2_ff2=0;
	wire init_wr_pulse;
	wire init_wr2_pulse;
	assign init_wr_pulse = init_wr_ff&&(!init_wr_ff2);
	assign init_wr2_pulse = init_wr2_ff&&(!init_wr2_ff2);
	always@(posedge clka or posedge rst)
	begin
		if(rst==1)
		begin
			init_wr_ff<='b0;
			init_wr_ff2<='b0;
			init_wr2_ff<='b0;
			init_wr2_ff2<='b0;
		end
		else begin
			init_wr_ff<=wr_start_first;
			init_wr_ff2<=init_wr_ff;
			init_wr2_ff<=wr_start_second;
			init_wr2_ff2<=init_wr2_ff;
		end
	end
	
	//д��ַ����
	always@(posedge clka or posedge rst)
	begin
		if(rst==1)
		begin
			addra<=0;
		end
		else
		begin
			if(PORTA_EN==1)
			begin
				addra<=addra+'b1;
			end
			else
			begin
				if(addra==13'b1_1111_1111_1111)
				begin
					addra<='b0;
				end
				else
					addra<=addra;
			end
		end
	end
	
	//����ַ����
	always@(posedge clkb or posedge rst)
	begin
		if(rst==1)
		begin
			addrb<=0;
		end
		else
		begin
			if(burst_delay_flag==1)
			begin
				addrb<=addrb+'b1;
			end
			else
			begin
				addrb<=addrb;
			end
		end
	end
	
	//�ɼ�ģ��״̬�����
	reg [2:0] status='b0;
	parameter IDLE = 0;
	parameter FIRST_WR = 1;
	parameter WR_WAITING = 2;
	parameter SECOND_WR = 3;
	parameter WR_DONE = 4;
	
	always@(posedge clka or posedge rst)
	begin
		if(rst==1)begin
			status<=IDLE;
			PORTA_EN<=0;
			full_flag<=0;
			half_full_flag<=0;
		end
		else begin
			case(status)
				IDLE://��ʼ״̬�����дʹ������
				begin
					if(init_wr_pulse==1)begin
						status<=FIRST_WR;
						PORTA_EN<=1;
					end
					else begin
						status<=status;
						PORTA_EN<=0;
						half_full_flag<=0;
					end
				end
				
				FIRST_WR://�ɼ���һ��ز��źţ����д��ַ
				begin
					if(addra==13'b0_1111_1111_1111)begin
						status<=WR_WAITING;
						PORTA_EN<=0;
						full_flag<=0;
						half_full_flag<=1;
					end
					else begin
						status<=status;
					end
				end
				
				WR_WAITING://�ȴ�״̬�����ڶ���дʹ������
				begin
					if(init_wr2_pulse==1)begin
						status<=SECOND_WR;
						PORTA_EN<=1;
					end
					else begin
						status<=status;
					end
				end
				
				SECOND_WR://�ɼ��ڶ���ز��źţ����д��ַ
				begin
					if(addra==13'b1_1111_1111_1111)begin
						status<=WR_DONE;
						PORTA_EN<=0;
						full_flag<=1;
						half_full_flag<=0;
					end
					else begin
						status<=status;
					end
				end
				
				WR_DONE://д��ɣ�ת���ʼ״̬
				begin
					status<=IDLE;
				end
				
				default:;
			endcase
		end
	end
	
	always@(posedge CLKA_DIV or posedge rst)
	begin
		if(rst==1)
		begin
			rd_en<='b0;
		end
		else begin
			if(full_flag==1)
			begin
				rd_en<=1;
			end
			else
			begin
				rd_en<=0;
			end
		end
	end
	
	//����BRAM
	blk_mem_gen_0 u_blk_mem_gen_0(
	.clka(clka),
	.ena(PORTA_EN),
	.wea(PORTA_WEA),
	.addra(addra),
	.dina(Data_in),
	.douta(),
	
	.clkb(clkb),
	.enb(burst_delay_flag),
	.web(PORTB_WEB),
	.addrb(addrb),
	.dinb(),
	.doutb(Data_out)
	);
    
endmodule
