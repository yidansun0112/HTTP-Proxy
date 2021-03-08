#include "cache.hpp"

std::mutex cache_mutex;

string Cache::get(string key){
  std::lock_guard<std::mutex> lck(cache_mutex);
  //if key does not exist, return NULL
  if (!mycache.count(key)) {
    return "";
  }
  //if exist, get node then move it to head
  Node* node = mycache[key];
  moveToHead(node);
  return node->value;
}

//put key and value pair into cache
void Cache::put(string key,string value){
  std::lock_guard<std::mutex> lck(cache_mutex);
  if (!mycache.count(key)) {
    // if key does not exist, create a new node
    Node* node = new Node(key, value);
    // add into hashmap
    mycache[key] = node;
    // add to head
    addToHead(node);
    ++size;
    if (size > capacity) {
      // if over capacity, remove tail node
      removeTail();
    }
  }
  else {
    // if key exist, get node and edit value then move it to head
    Node* node = mycache[key];
    node->value = value;
    moveToHead(node);
  }
}

//add node to head
void Cache::addToHead(Node *node){
  node->prev = head;
  node->next = head->next;
  head->next->prev = node;
  head->next = node;
}

//remove the given node
void Cache::removeNode(Node *node){
  node->prev->next = node->next;
  node->next->prev = node->prev;
}

//move the node to head
void Cache::moveToHead(Node *node){
  removeNode(node);
  addToHead(node);
}

//remove tail node and delete it
void Cache::removeTail(){
  Node* node = tail->prev;
  removeNode(node);
  mycache.erase(node->key);
  delete node;
  --size;
}

//copy constructor
Cache::Cache(const Cache & rhs):mycache(),capacity(rhs.capacity),size(0){
  //std::lock_guard<std::mutex> lck(cache_mutex);
  head=new Node();
  tail=new Node();
  head->next=tail;
  tail->prev=head;
  Node* curr=rhs.tail->prev;
  while(curr!=NULL&&curr->prev!=NULL){
    this->put(curr->key,curr->value);
    curr=curr->prev;
  }
}

//assignment overload
Cache& Cache::operator=(const Cache & rhs){
  //std::lock_guard<std::mutex> lck(cache_mutex);
  if(this!=&rhs){
    Cache temp(rhs);
    swap(temp.mycache,mycache);
    swap(temp.head,head);
    swap(temp.tail,tail);
    swap(temp.capacity,capacity);
    swap(temp.size,size);
  }
  return *this;
}


//destructor 
Cache::~Cache(){
  while(head!=NULL){
    Node* temp=head->next;
    delete head;
    head=temp;
  }
}

ostream& operator<< (ostream &s, const Cache& rhs){
  s<<"current size is "<<rhs.size<<endl;
  Cache::Node* curr=rhs.head;
  while(curr!=NULL){
    s<<"key is "<<curr->key<<" "<<" value is "<<curr->value<<endl;
    curr=curr->next;
  }
  s<<"end"<<endl;
  return s;
}


bool Cache::storeResponse(string uri,Response rsp,int id){
  string code=rsp.getCode();
  string cacheControl=rsp.getCacheControl();
  if(code=="200"&&!rsp.isPrivate()&&!rsp.isNoStore()){
    put(uri,rsp.getResponse());
    string expire=rsp.getExpire();
    string message;
    if(expire!=""){
      message=generateLogMsg(id,"cached, expires at "+expire);
    }
    else{
      message=generateLogMsg(id,"cached, but requires re-validation");
    }
    writeToLog(message);
    return true;
  }
  else{
    string reason="";
    if(code!="200"){
      reason="not 200 OK";
    }
    else if(rsp.isPrivate()){
      reason="Cache-control: private";
    }
    else if(rsp.isNoStore()){
      reason="Cache-control: no-store";
    }
    string message=generateLogMsg(id,"not cacheable because "+reason);
    writeToLog(message);
    return false;
  }
}

string Cache::revalidate(Request request, Response response,int socket,int id){
  string etag=response.getEtag();
  string lastModify=response.getLastModify();
  //if has e-tag, send if-none-match
  if(etag!=""){
    return checkIfNoneMatch(request,response,socket,etag,id);
  }
  //else if has last-modify, send if-modified-since
  else if(lastModify!=""){
    return checkIfModifiedSince(request,response,socket,lastModify,id);
  }
  //else send all request
  else{
    return reSendRequest(request,socket,id);
  }
}

string Cache::checkIfNoneMatch(Request request,Response response,int socket,string etag,int id){
  return checkValidate(request,response,socket,"If-None-Match: ",etag,id);
}


string Cache::checkIfModifiedSince(Request request,Response response,int socket,string lastModify,int id){
  return checkValidate(request,response,socket,"If-Modified-Since: ",lastModify,id);
}

string Cache::reSendRequest(Request request, int socket,int id){
  cout<<"resend"<<endl;
  string origin=request.getRequest();
  send(socket,&origin,sizeof(origin),0);
  string response=recvString(socket);
  Response newResponse(response);
  storeResponse(request.getURI(),newResponse,id);
  return newResponse.getResponse();
}

string Cache::checkValidate(Request request,Response response,int socket, string type,string content,int id){
  //send new request
  cout<<"re validate"<<endl;
  string origin=request.getHeader();
  string newRequest=origin+"\r\n"+type+content+"\r\n\r\n";
  sendString(socket,newRequest);
  //receive new response
  string nresponse=recvString(socket);
  Response newResponse(nresponse);
  if(newResponse.getCode()=="304"){
    return response.getResponse();
  }
  else{
    storeResponse(request.getURI(),newResponse,id);
    return newResponse.getResponse();
  }
}


