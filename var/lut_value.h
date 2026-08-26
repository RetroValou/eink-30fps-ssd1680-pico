#ifndef LUTVALUE
#define LUTVALUE

#include <cstdint>

class LUT {

    public :
        static const uint8_t v_black[153];
        static const uint8_t v_clean[153];
        static const uint8_t v_long[153];

        const uint8_t* v_diff;  
        const uint8_t* v_partialGrey;  

        static const uint8_t v_compensate[153];

        static const int size;  

    private : 
        static const uint8_t list_diff[4][153];


    public : 
        LUT() { updateLut(25);}

        void updateLut(int temperature);

};



#endif