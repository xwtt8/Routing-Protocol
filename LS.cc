#include "RoutingProtocolImpl.h"

//handle the situation when receive a packet with the type LS
void RoutingProtocolImpl::recvLS(unsigned short port, void *packet, unsigned short size) {
	char *pck = (char *)packet;
	unsigned short srcID = ntohs(*(unsigned short*)(pck + 4));
	unsigned int seq = ntohl(*(unsigned int*)(pck + 8));
	int i;

	printf("  receive LS table: from %d, sequence number: %d\n", srcID, seq);

	if (nodeVec.find(srcID) == nodeVec.end()) {
		printf("  This is a new LinkState table:\n");
		// add this new linkState
		linkState newLS;
		newLS.Node_ID = srcID;
		newLS.sequence = seq;
		newLS.update = sys->time();
		
		// insert the cost in the packet to linkState
		for (i = 0; i < size - 12; i += 4) {
			unsigned short nID = ntohs(*(unsigned short*)(pck + 12 + i));
			unsigned short ncost = ntohs(*(unsigned short*)(pck + 14 + i));
			printf("\tnode ID: %d, cost: %d\n", nID, ncost);
			newLS.neighbour.insert(pair<unsigned short, unsigned short>(nID, ncost));
		}
		
		nodeVec.insert(pair<unsigned short, linkState>(srcID, newLS));
		dijkstra();
		sendLSRecv(port, pck, size);
	}
	else if (nodeVec[srcID].sequence < seq) {
		printf("  Update current LinkState table: (old sequence number: %d; new sequence number: %d)\n", nodeVec[srcID].sequence, seq);
		// update this linkState
		linkState *ls = &nodeVec[srcID];
		ls->sequence = seq;
		ls->neighbour.clear();
		ls->update = sys->time();
		
		// insert the cost in the packet to linkState
		for (i = 0; i < size - 12; i += 4) {
			unsigned short nID = ntohs(*(unsigned short*)(pck + 12 + i));
			unsigned short ncost = ntohs(*(unsigned short*)(pck + 14 + i));
			printf("\tnode ID: %d, cost: %d\n", nID, ncost);
			ls->neighbour.insert(pair<unsigned short, unsigned short>(nID, ncost));
		}
		
		dijkstra();
		sendLSRecv(port, pck, size);
	}
	else
		printf("  This is an old LS table. Just ignore it.\n");

	packet = NULL;
	delete pck;
}

//send the received LS packet
void RoutingProtocolImpl::sendLSRecv(unsigned short port, char *packet, unsigned short size) {
	for (int i = 0; i < num_ports; i++)
		if (i != port) {
			char *toSend = (char*)malloc(sizeof(char) * size);
			*toSend = LS;
			*(unsigned short*)(toSend+2) = htons(size);
			*(unsigned short*)(toSend+4) = *(unsigned short*)(packet+4);
			*(unsigned int*)(toSend+8) = *(unsigned int*)(packet+8);
			for (int j = 0; j < size - 12; j += 4) {
				*(unsigned short*)(toSend+12+j) = *(unsigned short*)(packet + 12 + j);
				*(unsigned short*)(toSend+14+j) = *(unsigned short*)(packet + 14 + j);
			}
			sys->send(i, toSend, size);
		}
}

//perform the dijkstra Algorithm
void RoutingProtocolImpl::dijkstra() {

    int i;        // port_table.at(1).port_number = i
	set<unsigned short> nodeChecked;		    // nodes checked 
	set<unsigned short> nodeRemain;				// nodes not checked
	map<unsigned short, unsigned short> nodePair;		// neighbour nodes map: <nodeID, port number>
	map<unsigned short, unsigned short> currentCost;    // the cost of neighbour nodes map: <nodeID, cost>
	set<unsigned short>::iterator setit;			    // set iterator
	map<unsigned short, unsigned short>::iterator mapit;	// map iterator

	// initialize node maps
	nodeChecked.insert(router_id);
	for (i = 0; i < num_ports; i++)
		if (port_table.at(i).is_alive) {         
			unsigned short nodeID = port_table.at(i).neighbor_id;    // neighbours' IDs
			unsigned short nodeCost = port_table.at(i).cost;    // cost to neighbours
			if (nodeVec.find(nodeID) != nodeVec.end())	
				nodeRemain.insert(nodeID);		// insert neighbours into nodeRemain
			nodePair.insert(pair<unsigned short, unsigned short>(nodeID, i));
			currentCost.insert(pair<unsigned short, unsigned short>(nodeID, nodeCost));
		}
    
	// Dijkstra Algorithm
	while (!nodeRemain.empty()) {
		unsigned short minCost = INFINITY_COST;		// minimal cost to the current checked node
		unsigned short currentCheck;			// current checked node
		
		// get the node with the shortest cost from nodeRemain
		for (setit = nodeRemain.begin(); setit != nodeRemain.end(); setit++)
			if (currentCost[*setit] < minCost) {
				minCost = currentCost[*setit];
				currentCheck = *setit;
			}
			
		nodeRemain.erase(currentCheck);
		nodeChecked.insert(currentCheck);

		linkState *ls = &nodeVec[currentCheck];
		for (mapit = ls->neighbour.begin(); mapit != ls->neighbour.end(); mapit++) {
			if (nodeChecked.find(mapit->first) == nodeChecked.end()) {	// not in checked set
				if (currentCost.find(mapit->first) == currentCost.end()) {
					currentCost.insert(pair<unsigned short, unsigned short>(mapit->first, minCost + mapit->second));
					nodePair.insert(pair<unsigned short, unsigned short>(mapit->first, nodePair[currentCheck]));
					if (nodeVec.find(mapit->first) != nodeVec.end())
						nodeRemain.insert(mapit->first);	
				}
				else if (currentCost[mapit->first] > minCost + mapit->second) {
					// update the cost and the next node
					currentCost[mapit->first] = minCost + mapit->second;
					nodePair[mapit->first] = nodePair[currentCheck];
				}
			}
		}
	}
	
	//update the generic forwarding table
    	for (mapit = nodePair.begin(); mapit != nodePair.end(); mapit++) {
    	    unsigned short next_hop;
    	    for(i=0; i< (int)this->port_table.size(); i++) {
                if(this->port_table.at(i).port_num == mapit->second ) {
                        next_hop = this->port_table.at(i).neighbor_id;
                        break;
                }
            }  
	update_forwarding_table(mapit->first, next_hop);    // assumed updateForward
	}
}

//send the LS table to all neighbours
void RoutingProtocolImpl::sendLSTable(){
    char type = LS;
    unsigned short size;
    unsigned short sourceId = router_id;
    
    map<unsigned short, unsigned short> portInfo;
	
    //get neighbours
    for (int i = 0; i < num_ports; i++) {
        if (port_table.at(i).is_alive) {
            portInfo.insert(pair<unsigned short, unsigned short>(port_table.at(i).neighbor_id, port_table.at(i).cost));
        }
    }

    int sequenceNumber = mySequence;
    mySequence++;
    size = 12 + (portInfo.size() * 4);
    
	bool printed = false;
    for (int i = 0; i < num_ports; i++) {
        if (port_table.at(i).is_alive) {
		printf("\tSend LS table to port %d.\n", i);
            char * packet = (char *) malloc(sizeof(char) * size);
            *packet = type;
            *(short *)(packet + 2) = htons(size);
            *(short *)(packet + 4) = htons(sourceId);
            *(int *)(packet + 8) = htonl(sequenceNumber);
            
            int index = 12;
            for (map<unsigned short, unsigned short>::iterator it = portInfo.begin(); it != portInfo.end(); it++) {
                unsigned short neighbourID = it->first;
                unsigned short newCost = it->second;
		if (!printed)
			printf("neighbour ID: %d, cost: %d\n", neighbourID, newCost);
                *(short *)(packet + index) = htons(neighbourID);
                *(short *)(packet + index + 2) = htons(newCost);
                index += 4;
            }
		printed = true;
            sys->send(i, packet, size);
        }
    }
}

//check LS time out
void RoutingProtocolImpl::checkLSTimeOut() {
	for (map<unsigned short, linkState>::iterator it = nodeVec.begin(); it != nodeVec.end(); it++) {
		linkState ls = it->second;
		if (sys->time() - ls.update >= 45000)
			nodeVec.erase(ls.Node_ID);
	}
}