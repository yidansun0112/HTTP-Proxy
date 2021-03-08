#include "proxy.hpp"

int main(){
    Cache cache(20);
    int thread_id = 0;
    tryOpenFile();
    while(1){
        try{
            Server server(PORT);
            string ip_addr;
            int browser_fd = server.accept_connection(&ip_addr);
            cout<<ip_addr;
            thread_id++;
            Thread_info * info = new Thread_info();
            info->browser_fd = browser_fd;
            info->ip_addr = ip_addr;
            info->thread_id = thread_id;
            info->cache = &cache;
            pthread_t thread;
            pthread_create(&thread, NULL, process_request, info);
            //process_request(info);
        }catch(std::exception e){
            cerr<<e.what();
        }
    }
    return 0;
}