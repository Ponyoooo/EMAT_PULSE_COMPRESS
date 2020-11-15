`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2020/10/08 14:52:45
// Design Name: 
// Module Name: i2c_MCP4725
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


module i2c_MCP4725(
		input         rst,
		input         clk,
		inout         sda,
		output reg    scl
    );
    
parameter data = 16'h01D9;



reg sda_reg;//sda��map reg
reg en;//ʹ��
reg clk_100k_reg;
reg [7:0] cnt;

//������ַ���ֽڵ�ַ(h0a)����д������(h88)
wire [7:0]         DEVICEADDR = 8'b11000100;
wire [7:0]		    BYTEADDR = data[15:8];
wire [7:0]		    BYTEWRITE = data[7:0];
wire clk_100k;

assign clk_100k = clk_100k_reg;

//500����Ƶ��,����100k��ʱ��
reg[7:0] clk_count;
	always@(posedge clk or negedge rst)
	begin
		if(~rst)
		begin
			clk_100k_reg <= 0;
			clk_count <= 0;
		end
		else if (clk_count<8'd249)
		begin
		    clk_count <= clk_count+1'd1;
		end 
		else
		begin
			clk_100k_reg <= ~clk_100k_reg;
			clk_count <= 8'd0;
		end
	end
	
always @ (posedge clk_100k or negedge rst)	
begin
	if(!rst)
		begin
			en<=1'b1;//���
			sda_reg<=1'b1;
			scl<=1'b1;
			cnt<=0;
			//BYTEADDR = 0;
        // BYTEWRITE = 0;
		end
	else
		begin
		cnt<=cnt+1'b1;
		case (cnt)
			0:begin en<=1'b1;end//���
			
			1:begin scl<=1'b1;end//start
			2:begin sda_reg<=1'b1;end
			3:begin sda_reg<=1'b0;end
			
			4:begin scl<=1'b0;sda_reg <= DEVICEADDR[7];end
			5:begin scl<=1'b1;end	
			
			6:begin scl<=1'b0;sda_reg <= DEVICEADDR[6];end
			7:begin scl<=1'b1;end	
			
			8:begin scl<=1'b0;sda_reg <= DEVICEADDR[5];end
			9:begin scl<=1'b1;end	
			
			10:begin scl<=1'b0;sda_reg <= DEVICEADDR[4];end
			11:begin scl<=1'b1;end	
			
			12:begin scl<=1'b0;sda_reg <= DEVICEADDR[3];end
			13:begin scl<=1'b1;end	
			
			14:begin scl<=1'b0;sda_reg <= DEVICEADDR[2];end
			15:begin scl<=1'b1;end	
			
			16:begin scl<=1'b0;sda_reg <= DEVICEADDR[1];end
			17:begin scl<=1'b1;end			
			
			18:begin scl<=1'b0;sda_reg <= DEVICEADDR[0];end
			19:begin scl<=1'b1;end	
			20:begin scl<=1'b0;end//��scl�õͺ���ȥ����en
			
			21:begin en<=1'b0; end//sda����,׼������������ACK
			22:begin scl<=1'b1;end
			23:begin
				scl <= 1'b0;	
				if (1/*sda==1'b0*/) //ack
					begin cnt<=24;end//ָʾ��1��
				else
					begin cnt<=22;end
				end
		
			24:begin en<=1'b1;end//sda���
			25:begin scl<=1'b0;sda_reg=BYTEADDR[7];end//�ֽڵ�ַMSB
			26:begin scl<=1'b1;end
			
			27:begin scl<=1'b0;sda_reg=BYTEADDR[6];end
			28:begin scl<=1'b1;end
			
			29:begin scl<=1'b0;sda_reg=BYTEADDR[5];end
			30:begin scl<=1'b1;end
			
			31:begin scl<=1'b0;sda_reg=BYTEADDR[4];end
			32:begin scl<=1'b1;end
			
			33:begin scl<=1'b0;sda_reg=BYTEADDR[3];end
			34:begin scl<=1'b1;end
			
			35:begin scl<=1'b0;sda_reg=BYTEADDR[2];end
			36:begin scl<=1'b1;end
			
			37:begin scl<=1'b0;sda_reg=BYTEADDR[1];end
			38:begin scl<=1'b1;end
			
			39:begin scl<=1'b0;sda_reg=BYTEADDR[0];end//LSB
			40:begin scl<=1'b1;end
			41:begin scl<=1'b0;end
			
			42:begin en<=1'b0;end//sda����,׼������������ACK
			43:begin scl<=1'b1;end
			44:begin
				scl<=1'b0;
				if (1/*sda==1'b0*/) //ack
					begin cnt<=45;end//ָʾ��2��
				else
					cnt<=43;
				end
			45:begin en<=1'b1;end//sda���
			46:begin scl<=1'b0;sda_reg=BYTEWRITE[7];end//�ֽڵ�ַ��ָʾ��2��
			47:begin scl<=1'b1;end
			
			48:begin scl<=1'b0;sda_reg=BYTEWRITE[6];end
			49:begin scl<=1'b1;end
			
			50:begin scl<=1'b0;sda_reg=BYTEWRITE[5];end
			51:begin scl<=1'b1;end
			
			52:begin scl<=1'b0;sda_reg=BYTEWRITE[4];end
			53:begin scl<=1'b1;end
			
			54:begin scl<=1'b0;sda_reg=BYTEWRITE[3];end
			55:begin scl<=1'b1;end
			
			56:begin scl<=1'b0;sda_reg=BYTEWRITE[2];end
			57:begin scl<=1'b1;end
			
			58:begin scl<=1'b0;sda_reg=BYTEWRITE[1];end
			59:begin scl<=1'b1;end
			
			60:begin scl<=1'b0;sda_reg=BYTEWRITE[0];end
			61:begin scl<=1'b1;end
			62:begin scl<=1'b0;end
			
			63:begin en<=1'b0;end//sda����
			64:begin scl<=1'b1;end
			65:begin
				scl<=1'b0;
				if (1/*sda==1'b0*/) //ack
					begin cnt<=66;end//
				else
					cnt<=64;
				end
			
			66:begin en<=1'b1;end//sda���
			
			67:begin scl<=1'b1;end//stop
			68:begin sda_reg<=1'b0;end
			69:begin sda_reg<=1'b1;end
			
			default:;
			
		endcase
	end
end
assign sda = en?sda_reg:1'bz;	
endmodule
