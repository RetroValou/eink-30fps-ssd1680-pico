#ifndef LUTVALUE
#define LUTVALUE

#include <cstdint>

class LUT {

    public :
        static const uint8_t v_black[153];
        static const uint8_t v_clean[153];

        static const uint8_t v_diff[153];

        static const uint8_t v_compensate[153];

        static const int size;  


    public : 
        LUT(){};        
        ~LUT(){};

};



#endif