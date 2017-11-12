#include <string.h>
#include <limits.h>
#include <assert.h>
#include "cqt.h"
#include "cqt_net.h"
#include "../cqt_gen/cqt_gen.h"

#define LINE_BUF_SIZE (416)

int CQT_Conv2D_same_3x3_if_wf_of (CQT_LAYER *lp, void *inp, void *outp)
{
    float filter3x3[3*3];
    float bias;

    LY_Conv2D *cnvp;
    cnvp = lp->param_p;

    float *ip = (float *)inp;
    float *op = outp;
    float *wp = cnvp->weight_p;
    float *bp = cnvp->bias_p;

    int fill_num = cnvp->filters;
    int input_size_x;
    int input_size_y;
    int input_size_num;

    int f, x, y, n;
    int idx_i,idx_o;

    float data3x3[3][3];
    float o_data;
    int li; // for line buffer
    int write_buf_idx; //次に書き込むラインバッファ 2bit
    int read_buf_idx0; //読み出し位置のインデックス 2bit
    int read_buf_idx1; //読み出し位置のインデックス 2bit
    int read_buf_idx2; //読み出し位置のインデックス 2bit

    //line buffer
    static float line_buffer[LINE_BUF_SIZE][4]; // line-buffers


    float w_data;
    int last;

    input_size_x = lp->cqt_input_shape[1];  //画像サイズ
    input_size_y = lp->cqt_input_shape[2];  //画像サイズ
    input_size_num = lp->cqt_input_shape[3]; //入力の数

    //parameter check o_data
    assert(cnvp->kernel_size[0]==3);
    assert(cnvp->kernel_size[1]==3);
    assert(cnvp->padding==PD_SAME);
    assert(cnvp->strides[0]==1);
    assert(cnvp->strides[1]==1);
    assert(fill_num==lp->cqt_output_shape[3]);

    memset(op, 0.0, fill_num * input_size_y * input_size_x * sizeof(float));

    for(f=0;f<fill_num;f++) {
        for(n=0;n<input_size_num;n++){
            // get filter
            for(x=0;x<3;x++) {
                for(y=0;y<3;y++) {
                    idx_i = (f * input_size_num * 3 * 3) + (n * 3 * 3) + (y * 3) + x;
                    w_data = *(wp+idx_i);
                    filter3x3[(y * 3) + x] = w_data;
                }
            }

            if(cnvp->use_bias) {
                bias = *(bp+f);
            } else {
                bias = 0.0;
            }

            if (n==(input_size_num-1)) {
                last = 1;
            } else {
                last = 0;
            }

           idx_i = n * (input_size_y * input_size_x);
           idx_o = f * (input_size_y * input_size_x);

           //CQT_Conv2D_same_3x3_if_wf_of_hw(ip + idx_i, op  + idx_o, filter3x3, bias, 0, last);




            for(li=0;li<input_size_x;li++) {
                line_buffer[li][3] = 0;
            }

            for(li=0;li<input_size_x;li++) {
                line_buffer[li][1] = *(ip + idx_i);
                idx_i++;
            }

            for(li=0;li<input_size_x;li++) {
                line_buffer[li][2] = *(ip + idx_i);
                idx_i++;
            }

            write_buf_idx = 3;

            //apply filter
            for(y=0;y<input_size_y;y++) {
                for(x=0;x<input_size_x;x++) {
                    //get data
                    o_data = *(op + idx_o);

                    //メモリのラインバッファ選択
                    switch (y % 4) {
                        case 0:
                            read_buf_idx0 = 0;
                            read_buf_idx1 = 1;
                            read_buf_idx2 = 2;
                            break;
                        case 1:
                            read_buf_idx0 = 1;
                            read_buf_idx1 = 2;
                            read_buf_idx2 = 3;
                            break;
                        case 2:
                            read_buf_idx0 = 2;
                            read_buf_idx1 = 3;
                            read_buf_idx2 = 0;
                            break;
                        default:  //case 3
                            read_buf_idx0 = 3;
                            read_buf_idx1 = 0;
                            read_buf_idx2 = 1;
                            break;
                    }

                    // データ選択
                    data3x3[0][0] = (x==0) ? 0 : line_buffer[x-1][read_buf_idx0];
                    data3x3[0][1] = line_buffer[x][read_buf_idx0];
                    data3x3[0][2] = (x==(input_size_x - 1)) ? 0 : line_buffer[x+1][read_buf_idx0];

                    data3x3[1][0] = (x==0) ? 0 : line_buffer[x-1][read_buf_idx1];
                    data3x3[1][1] = line_buffer[x][read_buf_idx1];
                    data3x3[1][2] = (x==(input_size_x - 1)) ? 0 : line_buffer[x+1][read_buf_idx1];

                    data3x3[2][0] = (x==0) ? 0 : line_buffer[x-1][read_buf_idx2];
                    data3x3[2][1] = line_buffer[x][read_buf_idx2];
                    data3x3[2][2] = (x==(input_size_x - 1)) ? 0 : line_buffer[x+1][read_buf_idx2];

                    o_data += filter3x3[0] * data3x3[0][0];
                    o_data += filter3x3[3] * data3x3[0][1];
                    o_data += filter3x3[6] * data3x3[0][2];
                    o_data += filter3x3[1] * data3x3[1][0];
                    o_data += filter3x3[4] * data3x3[1][1];
                    o_data += filter3x3[7] * data3x3[1][2];
                    o_data += filter3x3[2] * data3x3[2][0];
                    o_data += filter3x3[5] * data3x3[2][1];
                    o_data += filter3x3[8] * data3x3[2][2];

                    if(last) {
                        o_data += bias;
                        //activattion
                        //if(act == ACT_RELU) {
                        //    if(o_data < 0) {
                        //        o_data = 0.0;
                        //    }
                        //}
                    }

                    *(op + idx_o) = o_data;
                    idx_o++;

                }
                //次のデータの書き込み
                //パラレル化可能
                if (y != (input_size_y - 1)) {
                    for(li=0;li<input_size_x;li++) {
                        line_buffer[li][write_buf_idx] = *(ip + idx_i);
                        idx_i++;
                    }
                } else {
                    for(li=0;li<input_size_x;li++) {
                        line_buffer[li][write_buf_idx] = 0;
                    }
                }

                if (write_buf_idx == 3) {
                    write_buf_idx = 0;
                } else {
                    write_buf_idx++;
                }
            }

        }
    }
    return CQT_RET_OK;
}

