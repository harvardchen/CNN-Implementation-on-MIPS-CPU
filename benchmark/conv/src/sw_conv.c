#include "printf.h"
#include "perf_cnt.h"
#include "trap.h"
#include "mul.h"
#include "div.h"

#define FRAC_BIT        10

#define RD_ADDR         135106448
#define RD_SIZE_D0      1
#define RD_SIZE_D1      1
#define RD_SIZE_D2      28
#define RD_SIZE_D3      28

#define WEIGHT_ADDR     134217728
#define WEIGHT_SIZE_D0  20      
#define WEIGHT_SIZE_D1  1
#define WEIGHT_SIZE_D2  5
#define WEIGHT_SIZE_D3  5

#define WR_ADDR         135108240
#define WR_SIZE_D0      1
#define WR_SIZE_D1      20
#define WR_SIZE_D2      12
#define WR_SIZE_D3      12

#define KERN_ATTR_CONV_PAD          0
#define KERN_ATTR_CONV_STRIDE       1
#define KERN_ATTR_POOL_PAD          0
#define KERN_ATTR_POOL_KERN_SIZE    2
#define KERN_ATTR_POOL_STRIDE       2


struct size_vec4 {
    unsigned d0;
    unsigned d1;
    unsigned d2;
    unsigned d3;
};

struct mem_addr {
    unsigned rd_addr;
    unsigned weight_addr;
    unsigned wr_addr;
};

int mul(short a,short b) {
    int ans = a*b;
    return ans;
}

struct mem_addr addr = {RD_ADDR, WEIGHT_ADDR, WR_ADDR};
struct size_vec4 rd_size = {RD_SIZE_D0, RD_SIZE_D1, RD_SIZE_D2, RD_SIZE_D3};
struct size_vec4 wr_size = {WR_SIZE_D0, WR_SIZE_D1, WR_SIZE_D2, WR_SIZE_D3};
struct size_vec4 weight_size = {WEIGHT_SIZE_D0, WEIGHT_SIZE_D1, WEIGHT_SIZE_D2, WEIGHT_SIZE_D3};
    
struct size_vec4 conv_size;



void convolution() {
    short* in = (short*)addr.rd_addr;
    short* weight = (short*)addr.weight_addr;
    short* out = (short*)addr.wr_addr;

    unsigned output_offset = 0;
    unsigned input_offset = 0;
    
    unsigned input_fm_w = rd_size.d3;
    unsigned input_fm_h = rd_size.d2;

    unsigned pad = KERN_ATTR_CONV_PAD;
    unsigned pad_len = pad << 1;
    
    unsigned conv_out_w = rd_size.d3 - weight_size.d3 + pad_len;
    unsigned conv_out_h = rd_size.d2 - weight_size.d2 + pad_len;

    unsigned stride = KERN_ATTR_CONV_STRIDE;

    unsigned no, ni, x, y, kx, ky;
    unsigned weight_offset = 0;
    unsigned weight_offset_store = 0;
    unsigned product_x_stride = 0;
    unsigned product_y_stride = 0;
    unsigned no_offset_1 = 0;
    unsigned no_offset_2 = 0;
    unsigned ni_offset_1 = 0;
    unsigned ni_offset_2 = 0;
    unsigned y_offset = 0;
    unsigned ky_offset_1 = 0;
    unsigned ky_offset_2 = 0;
    unsigned conv_size_area = mul(conv_size.d2,conv_size.d3);
    unsigned weight_size_area = mul(weight_size.d2,weight_size.d3);
    unsigned input_area = mul(input_fm_h,input_fm_w);

    int out_store;

    conv_out_w = div(conv_out_w, stride);
    conv_out_h = div(conv_out_h, stride);

    conv_out_w++;
    conv_out_h++;

    conv_size.d0 = wr_size.d0;
    conv_size.d1 = wr_size.d1;
    conv_size.d2 = conv_out_h;
    conv_size.d3 = conv_out_w;


    conv_size_area = mul(conv_size.d2,conv_size.d3);
    
    //TODO: Please add your own algorithm implementaion here

    for (no = 0; no < wr_size.d1; ++no)
    {
        no_offset_1 = mul(no,conv_size.d0);
        no_offset_2 = mul(no, conv_size_area);
        weight_offset_store = mul(no_offset_1,(1+mul(weight_size.d2,weight_size.d3)));

        for (y = 0; y < conv_size.d2; ++y)
        {
            product_y_stride = mul(stride,y);
            y_offset = mul(y,conv_size.d3);
            for (x = 0; x < conv_size.d3; ++x)
            {
                product_x_stride = mul(stride,x);
                out_store = 0;
                output_offset = no_offset_2 + y_offset + x;

                for (ni = 0; ni < conv_size.d0; ++ni)
                {
                    ni_offset_1 = mul(ni,(1+weight_size_area));
                    ni_offset_2 = mul(ni,input_area);
                    for (ky = 0; ky < weight_size.d2; ++ky)
                    {
                        ky_offset_1 = mul(weight_size.d3,ky);
                        ky_offset_2 = mul(input_fm_w,(product_y_stride + ky - pad));
                        for (kx = 0; kx < weight_size.d3; ++kx)
                        {
                            weight_offset = weight_offset_store + ni_offset_1 + 1 + ky_offset_1 + kx;                           
                            input_offset = ni_offset_2 + ky_offset_2 + product_x_stride + kx - pad;
                            if (((product_x_stride + kx) >= pad) && ((product_x_stride + kx) < (pad+input_fm_w)) 
                             && ((product_y_stride + ky) >= pad) && ((product_y_stride + ky) < (pad+input_fm_h)))
                            {
                                out_store += mul((short)(*(in+input_offset)), (short)(*(weight+weight_offset)));
                            }
                            
                        }
                    }
                }
                out_store = (((short)(out_store >> FRAC_BIT) & 0x7fff) | ((short)(out_store >> 16) & 0x8000)) + (*(weight+weight_offset_store));

                (*(out+output_offset)) = (short)out_store;
            }
        }
    }


}

void pooling() {
    short* out = (short*)addr.wr_addr;
    
    unsigned output_offset = 0;
    unsigned input_offset = 0;
    
    unsigned input_fm_w = conv_size.d3;
    unsigned input_fm_h = conv_size.d2;
    
    unsigned pad = KERN_ATTR_POOL_PAD;
    unsigned pad_len = pad << 1;
    
    unsigned pad_w_test = conv_size.d3 - KERN_ATTR_POOL_KERN_SIZE;
    unsigned pad_h_test = conv_size.d2 - KERN_ATTR_POOL_KERN_SIZE;

    unsigned pool_out_w = pad_w_test + pad_len;
    unsigned pool_out_h = pad_h_test + pad_len;

    unsigned stride = KERN_ATTR_POOL_STRIDE;

    unsigned pad_w_test_remain = pad_w_test - mul(div(pad_w_test, stride), stride);
    unsigned pad_h_test_remain = pad_h_test - mul(div(pad_h_test, stride), stride);

    short no,x,y,px,py;

    unsigned input_area = mul(input_fm_w,input_fm_h);
    unsigned pool_out_area = 0;
    unsigned y_offset = 0;
    unsigned no_offset_1 = 0;
    unsigned no_offset_2 = 0;
    unsigned product_y_stride = 0;
    unsigned product_x_stride = 0;
    unsigned product_2 = 0;


    pool_out_w = div(pool_out_w, stride);
    pool_out_h = div(pool_out_h, stride);
    pool_out_w++;
    pool_out_h++;

    if ( (!pad) && (pad_w_test_remain || pad_h_test_remain) )
    {
        pool_out_w++;
        pool_out_h++;
    }

    pool_out_area = mul(pool_out_w, pool_out_h);
    
    //TODO: Please add your own algorithm implementaion here
    for (no = 0; no < wr_size.d1; ++no)
    {
        no_offset_1 = mul(no,pool_out_area);
        no_offset_2 = mul(no,input_area);

        for (y = 0; y < pool_out_h; ++y)
        {
            y_offset = mul(y,pool_out_w);
            product_y_stride = mul(y,stride);
            for (x = 0; x < pool_out_w; ++x)
            {
                product_x_stride = mul(x,stride);
                output_offset = no_offset_1 + y_offset + x;

                for (py = 0; py < KERN_ATTR_POOL_KERN_SIZE; ++py)
                {
                    product_2 = mul(input_fm_w,(product_y_stride + py-pad));
                    for (px = 0; px < KERN_ATTR_POOL_KERN_SIZE; ++px)
                    {
                        input_offset = no_offset_2 + product_2 + product_x_stride+px - pad;
                        if (((product_x_stride + px) >= pad) && ((product_x_stride + px) < (pad+input_fm_w)) 
                         && ((product_y_stride + py) >= pad) && ((product_y_stride + py) < (pad+input_fm_h))
                         && (((*(out+output_offset)) < (*(out+input_offset))) || ((px == 0) && (py == 0))))
                        {
                            *(out+output_offset) = *(out+input_offset);
                        }
                    }
                }
            }
        }  
    }


}


int main()
{
    Result res;
    res.msec = 0;
    res.read_mem_cycle = 0;
    res.write_mem_cycle = 0;
    res.mem_cycle = 0;
    res.IF_cycle = 0;
    res.IW_cycle = 0;
    res.ID_EX_cycle = 0;
    res.RDW_cycle = 0;
    res.write_reg_file_cycle = 0;
    res.Load_cycle = 0;
    res.Store_cycle = 0;
    res.MUL_cycle = 0;
    res.R_type_cycle = 0;
    res.wait_cycle = 0;
    bench_prepare(&res);
    printf("starting convolution\n");
    convolution();
    printf("starting pooling\n");
    pooling();
    bench_done(&res);
    printf("Cycle cnt=%u\n", res.msec);
    printf("read_mem_cycle cnt=%u\n", res.read_mem_cycle);
    printf("write_mem_cycle cnt=%u\n", res.write_mem_cycle);
    printf("mem_cycle cnt=%u\n", res.mem_cycle);
    printf("IF_cycle=%u\n", res.IF_cycle);
    printf("IW_cycle=%u\n", res.IW_cycle);
    printf("ID_EX_cycle=%u\n", res.ID_EX_cycle);
    printf("RDW_cycle=%u\n", res.RDW_cycle);
    printf("write_reg_file_cycle=%u\n", res.write_reg_file_cycle);
    printf("Load_cycle=%u\n", res.Load_cycle);
    printf("Store_cycle=%u\n", res.Store_cycle);
    printf("MUL_cycle=%u\n", res.MUL_cycle);
    printf("R_type_cycle=%u\n", res.R_type_cycle);
    printf("wait_cycle=%u\n",res.wait_cycle);
    return 0;
}
