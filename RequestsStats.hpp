#include <mutex>
#include <sstream>
#ifndef __RequestsStats_H
#define __RequestsStats_H
class RequestsStats
{
    private:
        std::mutex mtx;
        unsigned long long resp_1xx;
        unsigned long long resp_2xx;
        unsigned long long resp_3xx;
        unsigned long long resp_4xx;
        unsigned long long resp_5xx;
    public:
        RequestsStats()
        {
            reset();
        }
        void incr(const int type){
            try {
                std::lock_guard<std::mutex> lck (mtx);
                switch(type){
                    case 1:
                        resp_1xx++;
                        break;
                    case 2:
                        resp_2xx++;
                        break;
                    case 3:
                        resp_3xx++;
                        break;
                    case 4:
                        resp_4xx++;
                        break;
                    case 5:
                        resp_5xx++;
                        break;
              }
            }
            catch (std::logic_error&) {
                std::cerr << "[mutex exception caught]\n";
            }
        }
        void reset()
        {
            try {
                std::lock_guard<std::mutex> lck (mtx);
                resp_1xx = 0LL;
                resp_2xx = 0LL;
                resp_3xx = 0LL;
                resp_4xx = 0LL;
                resp_5xx = 0LL;
            }
            catch (std::logic_error&) {
                std::cerr << "[mutex exception caught]\n";
            }
        }
        int write(char *buf){

            try {
                std::ostringstream buffer;
                std::lock_guard<std::mutex> lck (mtx);

                buffer << resp_1xx
                    << ","
                    << resp_2xx
                    << ","
                    << resp_3xx
                    << ","
                    << resp_4xx
                    << ","
                    << resp_5xx
                    << std::endl;
                memcpy(buf, buffer.str().c_str(), buffer.str().length());
                return buffer.str().length();
            }
            catch (std::logic_error&) {
                std::cerr << "[mutex exception caught]\n";
                return 0;
            }

        }
};
#endif
