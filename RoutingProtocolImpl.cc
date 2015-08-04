#include "RoutingProtocolImpl.h"

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  sys->time();
  printf("halo\n");
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  
  this->proto_type = protocol_type;
  this->router_id = router_id;
  this->num_ports = num_ports;
  short i;
  mySequence = 0;
  
  //cout <<"The protocol type is" << protocol_type << endl;;
  
  // create a port status table, set neighbor id to 0 and cost to infinity 
  for(i=0;i<num_ports;i++) {
        rport entry={i,0,INFINITY_COST,false};
        this->port_table.push_back(entry);
  }  
  // sending the initial ping-pong packet
  for(i=0;i<num_ports;i++) {
        this->detect_neighbor(router_id,i);  
  }

  char* data;
  data = (char*)malloc(sizeof(char));
  // '1' for 1s check, 'n' for neighbor update, 'd' for dv update, 'l' for ls update
  data[0] = '1';
  data[1] = 'n';
  data[2] = 'd';    
  data[3] = 'l';
  
  void* void_data1 = &data[0];
  sys->set_alarm(this,1000,void_data1);
  
  void* void_data2 = &data[1];
  sys->set_alarm(this,10000,void_data2);
  
  if(this->proto_type == P_DV) {
        void* void_data3 = &data[2];
        sys->set_alarm(this,30000, void_data3);
  } else {
        void* void_data3 = &data[3];
        sys->set_alarm(this,30000, void_data3);
  }
}

void RoutingProtocolImpl::handle_alarm(void *data) {
    char* flag = static_cast<char*>(data);
    int i;
    // enforce 1s check on dv table
    // enforce 1s check on port table
    if(*flag == '1') {
        // check whether DV table has timeout entries
        show_DV_table();
        //show_forwarding_table();
        for(i=0;i<(int)this->DV_table.size(); i++) {
                this->DV_table.at(i).ttl --;
                // erase the entry if its' TTL is <= 0
                if(this->DV_table.at(i).ttl <= 0) {
                  cout << "The DV entry for destination  " << this->DV_table.at(i).dest_id << " is time out\n";  
                  // delete the forwarding table entry to that dest id
                  int j;    
                  for(j=0;j<(int)this->forwarding_table.size();j++) {
                        if(this->forwarding_table.at(j).dest_id == this->DV_table.at(i).dest_id) {
                                this->forwarding_table.erase(this->forwarding_table.begin()+j);
                        }
                  }                
                  this->DV_table.erase(this->DV_table.begin()+i);
                }
        }
        // check whether port table has timeout ports
        bool change_flag = false;
        for(i=0;i<(int)this->port_table.size(); i++) {
                if(this->port_table.at(i).is_alive == false)
                        continue;
                else 
                {
                        this->port_table.at(i).ttl --;
                        if(this->port_table.at(i).ttl <=0) 
                        {
                                change_flag = true;
                                cout << "The port entry to node " <<this->port_table.at(i).neighbor_id 
                                << " of router "<< this->router_id << " is time out\n";
                                int j;
                                // delete the dv entry according to the port num:
                                
                                for(j=0;j<(int)this->DV_table.size();j++) {
                                        cout << "dest node: " <<  this->DV_table.at(j).dest_id 
                                        <<"port num is "<< this->DV_table.at(j).port_num<< endl;
                                }                              
                                unsigned short dest_id;
                                for(j=0;j<(int)this->DV_table.size();j++) {
                                        int e_flag = 0;
                                        if(this->DV_table.at(j).port_num == this->port_table.at(i).port_num) {
                                                dest_id = this->DV_table.at(j).dest_id;
                                                cout << "delete entry "<< dest_id<< endl;
                                                //this->DV_table.at(j).cost = INFINITY_COST;
                                                this->DV_table.erase(this->DV_table.begin()+j);
                                                e_flag = 1;
                                        }
                                        if(this->forwarding_table.at(j).next_hop == dest_id) {
                                                this->forwarding_table.erase(this->forwarding_table.begin()+j);
                                        }
                                        // if find a entry with the failed port, decrement the j value since we 
                                        // decrement the dv and forwarding table vector size 
                                        if(e_flag == 1)
                                                j--;
                                }
                        this->port_table.at(i).is_alive = false;
                        }
                }
        }
        if(this->proto_type == P_DV) {
                // send dv update to neighbors if the port table change
                if(change_flag) {
                        int j;
                        for(j=0;j<(int)this->port_table.size(); j++) {
                                if(this->port_table.at(j).is_alive == true) {
                                        unsigned short src = this->router_id;
                                        unsigned short dest = this->port_table.at(j).neighbor_id;
                                        unsigned short port_num = this->port_table.at(j).port_num;
                                        cout << "FORWARDING PACKET AFTER DETECT LINK FAILURE\n"; 
                                        forward_dv(src,dest,port_num,1);
                                }
                        }       
                }
        }
        else if(this->proto_type == P_LS) {
		// check timeout for LS
		checkLSTimeOut();  
		if (change_flag) {
				printf("\tLS table has updated. Flood the change.\n");
				sendLSTable();
				dijkstra();
		}
        }
        // check timeout for LS
        sys->set_alarm(this,1000, data);
    }
    // enforce 10s ping message update
    else if(*flag == 'n') {
        for(i=0;i<this->num_ports;i++) {
                this->detect_neighbor(this->router_id,i);  
        }
        sys->set_alarm(this,10000,data);
    }
    // enforce 30s check on dv update
    else if(*flag == 'd') {
        cout << "Sending DV update " << endl;
        for(i=0;i<(int)this->port_table.size(); i++) {
                // ensure the port does not fail
                if(this->port_table.at(i).is_alive == true)
                  forward_dv(router_id, this->port_table.at(i).neighbor_id, this->port_table.at(i).port_num,0); 
        }
        //show_DV_table();
        sys->set_alarm(this,30000, data);
    }
    else if(*flag == 'l') {
        cout <<"Sending LS update " << endl;
        sendLSTable();
        dijkstra();
        show_forwarding_table();
        sys->set_alarm(this,30000,data);
    } else{
        cout << "encounter unknown protocol" << endl;
    }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // void pointer can not be dereferenced, used for sending packet
  void* v_pointer; 
  if(port == SPECIAL_PORT) {

        struct sim_pkt_header* pk = static_cast<struct sim_pkt_header*>(packet);
        // forwarding the packet according to the DV table
        short src = (short)ntohs(pk->src);
        short dst = (short)ntohs(pk->dst);        
        forward_packet(src,dst);
        cout <<"FORWARD PACKET\n";
	free(packet);

        
  } else{
//        cout <<"The ID of this router is " << this->router_id << endl;
        char *p = static_cast<char*>(packet);
        ePacketType packet_type = (ePacketType)(*(ePacketType*)p); 
        char type = *(char *)packet;
        
        if(packet_type == DATA) {
        cout << "type is data\n";
        } else if(packet_type == PING) {
                // send a PONG message immediately
                *(ePacketType *) (p) = (ePacketType) PONG;
                *(short *) (p+48) = (*(short*)(p+32));
                *(short *) (p+32) = (short) htons(this->router_id);
                v_pointer = p;
                sys->send(port,v_pointer,size);  
                
        } else if(packet_type == PONG) {

                short sid = (short)ntohs(*(short*)(p+32));
                int time_stamp = (int)ntohl(*(int*) (p+64));
                int current_time = sys->time();
                // store the port status information
                update_port(port,sid,current_time - time_stamp);
                // update the DV table based on the neighbor information
                int is_change = 0;
                if(this->proto_type == P_DV) {
                        is_change = merge_route(sid,sid,port,current_time - time_stamp);
                        // send dv packet to neighbors if there is a change in the DV table
                        if(is_change !=0) {
                                cout << "IN THE PONG MESSAGE, DV CHANGE HAPPEN\n";
                                send_to_neighbor(0);
                        }
                }
		free(packet);                
                
        } else if(packet_type == DV) {
                
                short trigger_flag = (short)ntohs(*(short*)(p+8)); 
                short p_size = (short)ntohs(*(short*)(p+16));                       
                short sid = (short)ntohs(*(short*)(p+32));    
                int DV_num,i;          
                // get the port number for the neighbor 
                unsigned short port_num;
                int port_cost;
                for(i=0; i< (int)this->port_table.size(); i++) {
                        if(this->port_table.at(i).neighbor_id == sid) {
                                port_num = this->port_table.at(i).port_num;
                                port_cost = this->port_table.at(i).cost;
                                break;
                        }
                }               
                // number of node, cost pair
                DV_num = (p_size - 64)/32;
                
                int is_change = 0;
                // if the update is not from a link failure
                if(trigger_flag == 0) {
                       // cout <<"trigger is 0\n";
                        for(i=0;i<DV_num; i++) {
                                short node =(short)ntohs( *(short *)(p+64+i*32));
                                int cost = (int)ntohl(*(int *) (p+64+i*32+16));
                                // if dv table is updated return 1, otherwise return 0
                                is_change +=merge_route(node,sid,port_num,port_cost+cost);
                        }
                        // if local change is made, send it immediately to neighbors
                        if(is_change != 0) {
                                cout << "IN THE DV MESSAGE, DV CHANGE HAPPEN\n";
                                send_to_neighbor(0);
                        }                
                }
                // it is a triggered update, modify it's own DV and send dv to neighbor routers.
                else if(trigger_flag == 1)
                {  
                        short e_flag=0; // indicate whether a DV entry exist according to incoming DV packet 
                        short s_flag=0; // indicate whether a there is a change in the DV table, if so send 
                                       // additional DV packet to the neighbors
                        // iterate through the distance vector table
                        for(i=0;i<(int)this->DV_table.size();i++)
                        {                        
                                // check whether use it as a next hop
                                if(this->DV_table.at(i).next_hop == sid ) 
                                {
                                        //cout << "dest is " << this->DV_table.at(i).dest_id<< endl;; 
                                        for(int j=0;j<DV_num;j++) 
                                        {
                                                short node =(short)ntohs( *(short *)(p+64+j*32));
                                                if(this->DV_table.at(i).dest_id == node){
                                                        e_flag = 1;
                                                }
                                                if(this->DV_table.at(i).dest_id == sid)
                                                        e_flag = 1;
                                        }
                                
                                        // delete the failed entry
                                        if(e_flag == 0) 
                                        {
                                           s_flag = 1;
                                           /*
                                           cout << "receive router " << this->router_id << " dest "
                                           << this->DV_table.at(i).dest_id << "source router "
                                           << sid << endl;
                                           */
                                           int k;
                                           for(k=0;k<(int)this->forwarding_table.size();k++) {
                                                if(this->forwarding_table.at(k).dest_id ==
                                                        this->DV_table.at(i).dest_id) 
                                                {
                                                 this->forwarding_table.erase(this->forwarding_table.begin()+k);
                                                }               
                                           } 
                                           this->DV_table.erase(this->DV_table.begin()+i);       
                                        }
                                }
                                e_flag = 0;
                        }
                        // send the triggered dv update to neighbors if the router is modified
                        if(s_flag == 1) 
                        {
                          send_to_neighbor(1);
                        }
                        s_flag = 0;
                }
		free(packet);
		       
        } else if(type == LS) {
                recvLS(port,packet,size);
		free(packet);
        } else {
                cout << "Invalid packet type\n"; 
		free(packet);
        }
  
	}
}
void RoutingProtocolImpl::send_to_neighbor(short t) {
        int i;
        for(i=0;i<(int)this->port_table.size(); i++) 
        {
                if(this->port_table.at(i).is_alive == true) 
                {
                        unsigned short src = this->router_id;
                        unsigned short dest = this->port_table.at(i).neighbor_id;
                        unsigned short port_num = this->port_table.at(i).port_num;
                        forward_dv(src,dest,port_num,t);
                }
        }
}
void RoutingProtocolImpl::detect_neighbor( unsigned short router_id, unsigned short port_id) {
  char *packet;
  short pp_size = 3*32;
  void *v_pointer;
   
  // the packet consists of three 32bytes sub-packet
  packet = (char*) malloc(pp_size);
  // timestamp for ping message and it is stored in the packet payload
  int t = sys->time();

  *(ePacketType *) (packet) = (ePacketType) PING;
  *(short *) (packet+16) = (short) htons(pp_size);
  *(short *) (packet+32) = (short) htons(router_id);
  *(int *) (packet+64) = (int) htonl(t);

  v_pointer = packet;
  sys->send(port_id,v_pointer,pp_size);    		
}

void RoutingProtocolImpl:: update_port(unsigned short port, unsigned short neighbor, int cost) {
        // store the port status information
  for (vector<rport>::iterator it = port_table.begin() ; it != port_table.end(); ++it) {
        if((*it).port_num == port) {
                (*it).neighbor_id = neighbor;
                (*it).cost = cost;
                (*it).ttl = 15;
                (*it).is_alive = true;
        }
  }
}

void RoutingProtocolImpl::forward_dv( unsigned short src_id, unsigned short dest_id, unsigned short port_id,
unsigned short trigger) {

  char *packet;
  void *v_pointer;
  short router_num = this->DV_table.size();
  // packet type row + src,dest row + rows for every router
  short pp_size = 32*(2+router_num);
  packet = (char*)malloc(pp_size);
  
  *(ePacketType *) (packet) = (ePacketType) DV;
  *(short *) (packet+8) =  (short) htons(trigger);
  *(short *) (packet+16) = (short) htons(pp_size);
  *(short *) (packet+32) = (short) htons(src_id);
  *(short *) (packet+48) = (short) htons(dest_id);
  
  int i;
  for(i=0;i<(int)this->DV_table.size(); i++) {
        *(short *) (packet+64+i*32) = (short) htons(this->DV_table.at(i).dest_id);
        
        // poisoned reverse
        if(dest_id == this->DV_table.at(i).next_hop) {
                *(int *) (packet+64+i*32+16) = (int) htonl(INFINITY_COST);
        }
        *(int *) (packet+64+i*32+16) = (int) htonl(this->DV_table.at(i).cost);
  }
  v_pointer = packet;
  sys->send(port_id, v_pointer, pp_size);
}

int RoutingProtocolImpl::merge_route (unsigned short dest, unsigned short next, unsigned short port, int new_cost) {
  // 0 indicate the destination id is not find in the current routing table
  int i, find_flag = 0;
  // do not store information when the destination node is itself
  if(dest == this->router_id)
        return 0;
  
  for(i=0;i<(int)this->DV_table.size(); i++) {
        if(this->DV_table.at(i).dest_id == dest) {
                find_flag = 1;
                // update the new cost if received next hop is the same
                if((DV_table.at(i).next_hop == next) && (DV_table.at(i).cost != new_cost)) {
                        cout <<"SAME NEXT OLD COST NEW COST" << DV_table.at(i).cost <<"," << new_cost <<endl;
                        DV_table.at(i).cost = new_cost;
                        update_forwarding_table(dest,next);
                        return 1;
                }
                // update the routing table if the new cost+cost to neighbor is smaller
                if(DV_table.at(i).cost > new_cost) {
                        cout <<"DIFFERENT NEXT OLD COST NEW COST" << DV_table.at(i).cost <<"," << new_cost <<endl;
                        DV_table.at(i).cost = new_cost;
                        DV_table.at(i).next_hop = next;
                        DV_table.at(i).port_num = port;
                        this->DV_table.at(i).ttl = 45;       
                        update_forwarding_table(dest,next);
                        return 1;
                }
                // no change is made only refresh the ttl 
                else if(DV_table.at(i).cost == new_cost){
                        this->DV_table.at(i).ttl = 45; 
                }
                break;
        }
  }
  // the destination node id is not in the current forwarding table
  if(find_flag == 0) {
        dvtable entry={dest,next,port,new_cost,45};
        this->DV_table.push_back(entry); 
        update_forwarding_table(dest,next);
        cout <<"NEW ENTRY" << endl;
        return 1;
  }
  
  return 0;
}

void RoutingProtocolImpl::update_forwarding_table(unsigned short dest, unsigned short next) {
  if(this->forwarding_table.size() == 0) {
        ftable entry = {dest,next};
        this->forwarding_table.push_back(entry);
  }
  else {
        // 0 if the destination id is not in the current forwarding table
        short flag = 0;
        for (vector<ftable>::iterator it = forwarding_table.begin() ; it != forwarding_table.end(); ++it) {
                if((*it).dest_id == dest) {
                        (*it).next_hop = next;
                        flag =1;
                }
        }
        if(flag == 0) {
                ftable entry = {dest,next};
                this->forwarding_table.push_back(entry);
        }
  }
}

unsigned short RoutingProtocolImpl::get_port(unsigned short dst) {
        int i;
        unsigned short port_num;
        for(i=0; i< (int)this->DV_table.size(); i++) {
                if(this->DV_table.at(i).dest_id == dst) {
                        port_num = this->DV_table.at(i).port_num;
                        break;
                }
        }
        return port_num;  
}

void RoutingProtocolImpl::forward_packet(unsigned short src, unsigned short dst) {

  char *packet;
  void *v_pointer;
  short pp_size = 96;
  packet = (char*)malloc(pp_size);
  int i;
  unsigned short port_id=9999;
  
  cout <<"dest is " << dst << endl;
  
  if(this->proto_type == P_DV) {
        for(i=0;i<(int)this->DV_table.size();i++)
        {
        if(this->DV_table.at(i).dest_id == dst) {
                port_id = this->DV_table.at(i).port_num;
                cout << port_id << endl;
                break;
                }
        }  
  }
  // if it is LS protocol
  else {
        unsigned short next;
        for(i=0;i<(int)this->forwarding_table.size();i++)
        {
                if(this->forwarding_table.at(i).dest_id == dst){
                        next = this->forwarding_table.at(i).next_hop;
                        break;                
                }
        }
        for(i=0;i<(int)this->port_table.size();i++)
        {
                if(next == this->port_table.at(i).neighbor_id) {
                        port_id = this->port_table.at(i).port_num;
                        break;
                }
        }
  }

  
  *(ePacketType *) (packet) = (ePacketType) DATA;
  *(short *) (packet+16) = (short) htons(pp_size);
  *(short *) (packet+32) = (short) htons(src);
  *(short *) (packet+48) = (short) htons(dst);
  cout <<"port num is "<<port_id<<endl;
  v_pointer = packet;
  
  if(port_id != 9999)
  sys->send(port_id, v_pointer, pp_size);
}

void RoutingProtocolImpl::show_port_table() {
  int i;
  cout <<"Router " << this->router_id <<" current port table is:" << endl;
  for(i=0;i<(int)this->port_table.size(); i++) {
        cout << "Port ID is " <<  this->port_table.at(i).port_num 
             << " neighbor ID is " << this->port_table.at(i).neighbor_id 
             << " port number is" <<this->port_table.at(i).cost
             << " is alive is" << this->port_table.at(i).is_alive << endl;
  }  
}

void RoutingProtocolImpl::show_DV_table() {
  int i;
  cout <<"Router " << this->router_id <<" current DV table is:" <<endl;
  for(i=0;i<(int)this->DV_table.size(); i++) {
        cout << "destination ID is " <<  this->DV_table.at(i).dest_id 
             << " next hop is " << this->DV_table.at(i).next_hop 
             << " port number is" <<this->DV_table.at(i).port_num
             <<"  cost is " << this->DV_table.at(i).cost
             <<" TTL is "<<this->DV_table.at(i).ttl << endl;
  }
}

void RoutingProtocolImpl:: show_forwarding_table() {
  int i; 
  cout <<"Router " << this->router_id <<" current forwarding table is:" << endl;
  for(i=0;i<(int)this->forwarding_table.size(); i++) {
        cout << "destination ID is " <<  this->forwarding_table.at(i).dest_id 
             << " next hop is " << this->forwarding_table.at(i).next_hop << endl;
  }
}