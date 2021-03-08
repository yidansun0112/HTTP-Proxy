#include "proxy.hpp"
#include <thread>
pthread_mutex_t thread_mtx = PTHREAD_MUTEX_INITIALIZER;
string check502(string response){
    string bad="HTTP/1.1 502 Bad Gateway\r\n\r\n";
    size_t f1=response.find("HTTP/1.1");
    if(f1==string::npos){
        return bad;
    }
    // if(response.find("\r\n\r\n")==string::npos){  
    //     return bad;
    // }
    return "";
}

string getResponseGet(Request request,Cache &cache,int thread_id){
    string hostname=request.getHostname();
    string port=request.getPort();
    Client client(hostname.c_str(),port.c_str()); 
    int client_socket=client.socket_fd;
    string uri=request.getURI();
    string response=cache.get(uri);
    if(response!=""){
        cout<<"in cache"<<endl;
        Response rsp(response);
        if(rsp.needRevalidate(thread_id)){
            response=cache.revalidate(request,rsp,client_socket,thread_id);
        }
        else{
            string message=generateLogMsg(thread_id,"in cache, valid");
            writeToLog(message);
        }
    }
    else{
        cout<<"not in cache"<<endl;
        string message=generateLogMsg(thread_id,"not in cache");
        writeToLog(message);
        string requestContent = request.getRequest();
        //cout<<requestContent<<endl;
        sendString(client_socket,requestContent);
        writeRequestLog(request.getFirstLine(),hostname,thread_id);
        vector<char> r;
        client.my_recv(r);
        // response=recvString(client_socket);
        // cout<<"received"<<endl;
        //cout<<response<<endl;
        // string bad=check502(response);
        // if(bad!=""){
        //     return bad;
        // }
        Response rsp(r);
        writeReceiveLog(rsp.getFirstLine(),hostname,thread_id);
        response=rsp.getResponse();
        cache.storeResponse(uri,rsp,thread_id);
        cout<<"store in cache"<<endl;
    }
    return response;
}

void stay_connect(int browser_fd, int server_fd, int thread_id) {
    send(browser_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
    string logMsg = generateLogMsg(thread_id,"Responding \"HTTP/1.1 200 OK\"");
    writeToLog(logMsg);
    fd_set readfds;
    int nfds = server_fd > browser_fd ? server_fd + 1 : browser_fd + 1;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(browser_fd, &readfds);

        select(nfds, &readfds, NULL, NULL, NULL);
        int fd[2] = {server_fd, browser_fd};
        int len;
        for (int i = 0; i < 2; i++) {
            char message[65536] = {0};
            if (FD_ISSET(fd[i], &readfds)) {
                len = recv(fd[i], message, sizeof(message), 0);
                if (len <= 0) {
                    return;
                }else {
                    if (send(fd[1 - i], message, len, 0) <= 0) {
                        return;
                    }
                }
            }
        }
    }
}

void post(int browser_fd, int server_fd, Request request, int thread_id){
    string req_msg = request.getWholeRequest();
    int post_len = getLength(req_msg);  //get length of client request
    if (post_len != -1) {
        // cout<<endl<<endl<<"Request_msg sent:"<<endl<<req_msg<<endl<<endl;
        // std::string request = recvWithLen(browser_fd, req_msg, post_len);
        // cout<<endl<<endl<<"Request sent:"<<endl<<request<<endl<<endl;
        sendString(server_fd,req_msg);
        // char send_request[request.length() + 1];
        // strcpy(send_request, request.c_str());
        // send(server_fd,send_request,sizeof(send_request),MSG_NOSIGNAL);  // send all the request info from client to server
        char response[65536] = {0};
        int response_len = recv(server_fd,response,sizeof(response),MSG_WAITALL);  //first time received response from server
        if (response_len != 0) {
            Response res;
            //res.ParseLine(req_msg, len);   
            //logFile << id << ": Received \"" << res.getLine() << "\" from " << host << std::endl;
            std::cout << "receive response from server which is:" << response << std::endl;
            send(browser_fd, response, response_len, 0);
            //logFile << id << ": Responding \"" << res.getLine() << std::endl;
        }else {
            std::cout << "server socket closed!\n";
        }
    }
}

void *process_request(void * information){
    Thread_info *info = (Thread_info *)information;
    int browser_fd = info->browser_fd;
    string ip_addr = info->ip_addr;
    int thread_id = info->thread_id;
    cout<<browser_fd<<endl;
    cout<<ip_addr<<endl;
    cout<<thread_id<<endl;

    Cache *cache = info->cache;
    char request[65536] = {0};
    int len = recv(browser_fd, request, sizeof(request),0);
    if(len<=0){
        std::string errMsg = "WARNING invalid request from "+ip_addr+" @ "+currTime();
        string log_msg = generateLogMsg(thread_id,errMsg);
        writeToLog(log_msg);
        return NULL;
    }
    string req = string(request,len);
    cout<<"Thread: "<<thread_id<<" is created"<<endl;
    

    Request p(req);
    if(p.getRequest()==req){
        cout<<"equals"<<endl;
    }
    else{
        cout<<"not equal"<<endl;
    }
    string method = p.getMethod();
    string hostname=p.getHostname();
    string port=p.getPort();
    cout<<req<<endl;
    if(p.isValid()){
        std::string errMsg = p.getFirstLine()+" from "+ip_addr+" @ "+currTime();
        string log_msg = generateLogMsg(thread_id,errMsg);
        writeToLog(log_msg);
    }else{
        std::string errMsg = std::string("Responding \"HTTP/1.1 400 Bad Request\"")+" @ "+currTime();
        sendString(browser_fd,errMsg);
        string log_msg = generateLogMsg(thread_id,errMsg);
        writeToLog(log_msg);
    }
    if(method=="GET"){
        try{
            cout<<"try get response"<<endl;
            string response=getResponseGet(p,*cache,thread_id);
            //cout<<response<<endl;
            Response rsp(response);
            string message=generateLogMsg(thread_id,"Responding "+rsp.getFirstLine());
            writeToLog(message);
            sendString(browser_fd,response);
            cout<<"response send"<<endl;
        } 
        // try{
        //     cout<<port<<endl;
        //     Client client(hostname.c_str(),port.c_str());
        //     send(client.socket_fd,request,len,0);
        //     vector<char> rsp;
        //     client.my_recv(rsp);
        //     my_sendTo(browser_fd,rsp);
        //     // string response=recvString(client.socket_fd);
        //     // sendString(browser_fd,response);
        // }
        catch(MyException e){
            string log_msg = generateLogMsg(thread_id, e.what());
            writeToLog(log_msg);
        }
    }else if(method=="CONNECT"){
        try{
            cout<<port<<endl;
            Client client(hostname.c_str(),port.c_str());
            stay_connect(browser_fd,client.socket_fd,thread_id);
            string log_msg = generateLogMsg(thread_id, "Connection closed");
            writeToLog(log_msg);
        }
        catch(MyException e){
            string log_msg = generateLogMsg(thread_id, e.what());
            writeToLog(log_msg);
        }
    }else if(method=="POST"){
        Client client(hostname.c_str(),port.c_str());
        post(browser_fd,client.socket_fd,p,thread_id);
    }
    return NULL;
}


