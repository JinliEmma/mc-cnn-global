#ifndef QING_BIN_READER_H
#define QING_BIN_READER_H

//@ranqing

#include "../../../Qing/qing_common.h"
#include "../../../Qing/qing_string.h"
#include "../../../Qing/qing_timer.h"
#include "../../../Qing/qing_dir.h"
#include "../../../Qing/qing_file_reader.h"
#include "../../../Qing/qing_file_writer.h"
#include "../../../Qing/qing_check.h"
#include "../../../Qing/qing_macros.h"
#include "../../../Qing/qing_matching_cost.h"
#include "../../../Qing/qing_bilateral_filter.h"

#define STEREO_RIGHT 1

class qing_mcost_to_disp {
public :
    qing_mcost_to_disp(const int d, const int h, const int w) ;
    ~qing_mcost_to_disp() ;

    void read_image(const string filename_l, const string filename_r);

    void read_from_mc_cnn_using_example_code(const string filename_l, const string filename_r);
    void read_from_mc_cnn(const string filename_l, const string filename_r );
    void read_from_mc_cnn_cmp(const string filename_l, const string filename_r);
    void remove_mcost_nan();

    void get_weighted_table(float sigma_range, float sigma_spatial);
    void mcost_to_disp(const int scale, const string savename);

    void mcost_aggregation(const int wnd);
    void adaptive_weight_filter(float *out, float *in, uchar * gray_l, uchar * gray_r, int d, int wnd);
    void directional_mcost_aggregation(const int wnd);

    void semi_global();

    void scanline_optimize();
    void scanline_optimize_x(float * so_mcost_vol, float * in_mcost_vol, int xc, int r);
    void scanline_optimize_y(float * so_mcost_vol, float * in_mcost_vol, int yc, int r);
    void scanline_optimize_x_sub(float * so_mcost_vol, float * in_mcost_vol, int x1, int x2 );
    void scanline_optimize_y_sub(float * so_mcost_vol, float * in_mcost_vol, int y1, int y2 );
    void scanline_optim(float * so_mcost_vol, float * in_mcost_vol, int x1, int x2, int y1, int y2);
    void scanline_optim_params(int x1, int x2, int y1, int y2, int d, float& P1, float& P2);

    void save_filtered_mcost(const string folder);
    void read_filtered_mcost(const string folder);

private:
    float * m_mcost_l, * m_mcost_r;                                    //matching cost
    float * m_filtered_mcost_l, * m_filtered_mcost_r;                  //filtered matching cost
    uchar * m_image_l, * m_image_r;                                    //left and right image
    unsigned char * m_disp_l, * m_disp_r, * m_disp;                    //disparity image
    int m_total_size, m_image_size;                                    //total_size = d * h * w, image_size = h * w
    int m_d, m_h, m_w, m_channels;                                     //d: disparity_range, h: image height, w: image width

    float * m_range_table;
    float * m_spatial_table;

    Mat m_mat_l, m_mat_r;
};

inline qing_mcost_to_disp::qing_mcost_to_disp(const int d, const int h, const int w): m_d(d), m_h(h), m_w(w) {
    m_total_size = m_d * m_h * m_w;
    m_image_size = m_h * m_w;

    m_mcost_l = 0;
    m_mcost_r = 0;
    m_filtered_mcost_l = 0;
    m_filtered_mcost_r = 0;

    m_disp_l = new unsigned char[m_image_size];
    m_disp_r = new unsigned char[m_image_size];
    m_disp = new unsigned char[m_image_size];
    if(0==m_disp_l || 0==m_disp_r) {
        cerr << "failed to initial disparity images." << endl;
        exit(-1);
    }
}

inline qing_mcost_to_disp::~qing_mcost_to_disp() {
    //    if(0!=m_mcost_l) { delete [] m_mcost_l;}
    //    if(0!=m_mcost_r) { delete [] m_mcost_r;}
    //    if(0!=m_disp_l)  { delete [] m_disp_l; }
    //    if(0!=m_disp_r)  { delete [] m_disp_r; }
}


inline void qing_mcost_to_disp::read_image(const string filename_l, const string filename_r) {
    m_mat_l = imread(filename_l, CV_LOAD_IMAGE_UNCHANGED);
    if(0==m_mat_l.data) {
        cerr << "failed to open " << filename_l << endl;
        return ;
    }
    m_mat_r = imread(filename_r, CV_LOAD_IMAGE_UNCHANGED);
    if(0==m_mat_r.data) {
        cerr << "failed to open " << filename_r << endl;
        return ;
    }
    m_channels = m_mat_l.channels();
    cout << m_mat_l.channels() << endl;

    m_image_l = new uchar[m_image_size * m_channels];
    m_image_r = new uchar[m_image_size * m_channels];
    if(0==m_image_l || 0==m_image_r) {
        cerr << "failed to initial bgr images." << endl;
        exit(-1);
    }
    memcpy(m_image_l, m_mat_l.data, sizeof(uchar) * m_image_size * m_channels);
    memcpy(m_image_r, m_mat_r.data, sizeof(uchar) * m_image_size * m_channels);

# if 1
    //cout << "COOL" << endl;
    Mat test_mat_l(m_h, m_w, m_mat_l.type());
    Mat test_mat_r(m_h, m_w, m_mat_r.type());
    memcpy(test_mat_l.data, m_image_l, sizeof(uchar) * m_image_size * m_channels);
    memcpy(test_mat_r.data, m_image_r, sizeof(uchar) * m_image_size * m_channels);
    imshow("test_mat_l", test_mat_l);
    imshow("test_mat_r", test_mat_r);
    waitKey(0);
    destroyAllWindows();
# endif
}


//@read matching cost files ".bin" to ".txt"
inline void qing_mcost_to_disp::read_from_mc_cnn(const string filename_l, const string filename_r) {

    assert(qing_check_file_suffix(filename_l, ".bin")==true &&
           qing_check_file_suffix(filename_r, ".bin")==true);

    m_mcost_l = new float[m_total_size];
    if(0==m_mcost_l) {
        cerr << "bad alloc in left cost volume.." << endl;
        exit(-1);
    }
    qing_read_bin(filename_l, m_mcost_l, m_total_size) ;

# if STEREO_RIGHT
    m_mcost_r = new float[m_total_size];
    if(0==m_mcost_r) {
        cerr << "bad alloc in right cost volume.." << endl;
        exit(-1);
    }
    qing_read_bin(filename_r, m_mcost_r, m_total_size) ;
# endif

# if 0
    qing_create_dir("matching_cost");
    for(int d = 0; d < m_d; ++d) {
        float * mcost = m_mcost_l + d * m_image_size;
        string out_file = "matching_cost/qing_mcost_l_" + qing_int_2_string(d) + ".jpg";
        qing_save_mcost_jpg(out_file, mcost, m_w, m_h);
    }
# endif
# if 0
    string out_file_l = getFilePrefix(filename_l) + ".txt";
    string out_file_r = getFilePrefix(filename_r) + ".txt";
    qing_write_txt(out_file_l, m_mcost_l, m_total_size, m_h * m_w);
    qing_write_txt(out_file_r, m_mcost_r, m_total_size, m_h * m_w);
# endif
}

inline void qing_mcost_to_disp::read_from_mc_cnn_cmp(const string filename_l, const string filename_r) {
    bool flag = false;
    float * temp_mcost_l = new float[m_total_size];
    qing_read_bin(filename_l, temp_mcost_l, m_total_size);
    for(int i = 0; i < m_total_size; ++i) {
        if(qing_is_nan(temp_mcost_l[i])==false && temp_mcost_l[i] < m_mcost_l[i]) {
            m_mcost_l[i] = temp_mcost_l[i];
            if(flag==false) {cout<<"HHH"<<endl;flag=true;}
        }
    }
# if STEREO_RIGHT
    qing_read_bin(filename_r, temp_mcost_l, m_total_size);
    for(int i = 0; i < m_total_size; ++i) {
        if(qing_is_nan(temp_mcost_l[i])==false && temp_mcost_l[i] < m_mcost_r[i]) {
            m_mcost_r[i] = temp_mcost_l[i];
        }
    }
# endif
}

inline void qing_mcost_to_disp::read_from_mc_cnn_using_example_code(const string filename_l, const string filename_r) {
    m_mcost_l = new float[m_total_size];
    if(0==m_mcost_l) {
        cerr << "bad alloc in left cost volume.." << endl;
        exit(-1);
    }
    int fd_l = open(filename_l.c_str(), O_RDONLY);
    if(-1==fd_l) {
        cerr << "failed to open " << filename_l;
        exit(-1);
    }

    //m_mcost_l = (float *)mmap(NULL, 1*m_d*m_w*m_h*sizeof(float), PROT_READ, MAP_SHARED, fd_l, 0);    //fd: file descriptor
    m_mcost_l = (float *)mmap(NULL, 1*m_d*m_w*m_h*sizeof(float), PROT_WRITE, MAP_PRIVATE, fd_l, 0);    //fd: file descriptor, not related to the original file
    close(fd_l);

#if STEREO_RIGHT
    m_mcost_r = new float[m_total_size];
    if(0==m_mcost_r) {
        cerr << "bad alloc in right cost volume.." << endl;
        exit(-1);
    }
    int fd_r = open(filename_r.c_str(), O_RDONLY);
    if(-1==fd_r) {
        cerr << "failed to open " << filename_r;
        exit(-1);
    }
    //m_mcost_r = (float *)mmap(NULL, 1*m_d*m_w*m_h*sizeof(float), PROT_READ, MAP_SHARED, fd_r, 0);
    m_mcost_r = (float *)mmap(NULL, 1*m_d*m_w*m_h*sizeof(float), PROT_WRITE, MAP_PRIVATE, fd_r, 0);
    close(fd_r);
# endif
}

inline void qing_mcost_to_disp::save_filtered_mcost(const string folder) {
    string filename;

//    filename = folder + "/left.txt" ;
//    qing_save_mcost_vol(filename, m_filtered_mcost_l, m_d, m_h, m_w);
    filename = folder + "/left.bin";
    qing_write_bin(filename, m_filtered_mcost_l, m_total_size);

# if STEREO_RIGHT
//    filename = folder + "/right.txt";
//    qing_save_mcost_vol(filename, m_filtered_mcost_r, m_d, m_h, m_w);
    filename = folder + "/right.bin";
    qing_write_bin(filename, m_filtered_mcost_r, m_total_size);
# endif
}

inline void qing_mcost_to_disp::read_filtered_mcost(const string folder) {
    string filename = folder + "/left.bin";

    m_filtered_mcost_l = new float[m_total_size];
    if(0==m_filtered_mcost_l) {
        cerr << "bad alloc in left cost volume.." << endl;
        exit(-1);
    }
    qing_read_bin(filename, m_filtered_mcost_l, m_total_size) ;
    cout << "reading " << filename << " done.." << endl;

# if STEREO_RIGHT
    filename = folder + "/right.bin";
    m_filtered_mcost_r = new float[m_total_size];
    if(0==m_filtered_mcost_r) {
        cerr << "bad alloc in right cost volume.." << endl;
        exit(-1);
    }
    qing_read_bin(filename, m_filtered_mcost_r, m_total_size) ;
    cout << "reading " << filename << " done.." << endl;
# endif
}

inline void qing_mcost_to_disp::remove_mcost_nan() {
    cout << "remove nan in matching cost...\t" ;
    float * ptr_mcost = m_mcost_l;
    int cnt = 0;
    for(int i = 0; i < m_total_size; ++i, ++ptr_mcost) {
        float mcost = *ptr_mcost;
        if(qing_is_nan(mcost)) {
            *ptr_mcost = 0.f;
            cnt ++;
        }
    }
    cout << cnt << " nan..." << endl;
}

inline void qing_mcost_to_disp::get_weighted_table(float sigma_range, float sigma_spatial) {
    sigma_range *= QING_FILTER_INTENSITY_RANGE;
    sigma_spatial *= min(m_w, m_h);
    cout << "sigma_range = " << sigma_range << endl;
    cout << "sigma_spatial = " << sigma_spatial << endl;
    m_range_table = qing_get_range_weighted_table(sigma_range, QING_FILTER_INTENSITY_RANGE);
    m_spatial_table = qing_get_spatial_weighted_table(sigma_spatial, QING_FILTER_SPATIAL_RANGE);
}


#endif // QING_BIN_READER_H
