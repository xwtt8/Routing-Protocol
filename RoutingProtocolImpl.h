#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "Simulator.h"
#include "Node.h"
#include <map>
#include <set>
#include <vector>
#include <netinet/in.h>

  
class RoutingProtocolImpl : public RoutingProtocol {

public:

  // Forwarding table
  struct forwardTable {
        unsigned short dest_id;
        unsigned short next_hop;
  };
  typedef struct forwardTable ftable;
  
  // port table
  struct port {
        unsigned short port_num;
        unsigned short neighbor_id;
        int cost;
        bool is_alive;
        int ttl;
  };
  typedef struct port rport; 
  
  // DV table
  struct DVtable { 
        unsigned short dest_id;
        unsigned short next_hop;
        unsigned short port_num;
        int cost; 
        int ttl;
  }; 
  typedef struct DVtable dvtable;
  
  struct _linkState;

  typedef struct _linkState {
    unsigned int sequence;
    unsigned short Node_ID;
	unsigned int update;
    map<unsigned short, unsigned short> neighbour;
  } linkState;
  
  // added code:
  
  
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.
    
    


 private:
        // To store Node object; used to access GSR9999 interfaces 
        Node *sys; 
        
        // The total number of ports on this router
        short num_ports; 
        
        // The type of selected protocol
        eProtocolType proto_type;
        
        // The ID of this router
        unsigned short router_id;
    
        // Vector for storing port status
        vector<rport>  port_table;
        
        // Vector for storing DV information
        vector<dvtable> DV_table;
        
        // Vector for storing forwarding table information
        vector<ftable> forwarding_table;
        
        // Sequence number in the LS update packet
        unsigned int mySequence;		
	
	// Map of Link State table
	map<unsigned short, linkState> nodeVec;
        
        void send_to_neighbor(short t);
        
        // send ping message for detecting neighbor nodes
        void detect_neighbor(unsigned short router_id, unsigned short port_id);
        
        // Forwarding DV update to the neighbors, trigger 0 if periodic update, 1 if triggered update
        void forward_dv( unsigned short src_id, unsigned short dest_id, unsigned short port_id,
                         unsigned short trigger);
        
        // Update port status
        void update_port(unsigned short port, unsigned short neighbor, int cost);
        
        // Update DV table based on new DV update or new port status
        int merge_route(unsigned short dest, unsigned short next, unsigned short port, int cost);
        
        // Update the forwarding table based on the DV table
        void update_forwarding_table(unsigned short dest, unsigned short next_hop);
        
        // Get the port number from neighbor id
        unsigned short get_port(unsigned short dst);
        
        // Send data packet to destination node
        void forward_packet(unsigned short src, unsigned short dst);
        
        void show_DV_table();
        
        void show_port_table();
        
        void show_forwarding_table();
                
	// update status (tables) for Link State Protocol
	bool LSUpdate();
	
	// Deal with LS packet
	void recvLS(unsigned short port, void *packet, unsigned short size);
	
	// get shortest path by using Dijkstra algorithm
	void dijkstra();
	
	// forward LS packet that is received from others
	void sendLSRecv(unsigned short port, char *packet, unsigned short size);
	
	// flood LS packet to all alive neighbours
	void sendLSTable();

	//check LS time out
	void checkLSTimeOut();
	 
};

#endif

