/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 *          Tushar Krishna
 */
 /*************************************This is NI v7 trying to use Huawei's model: multiple task queue to forward flit and also send prefetch to memory***************************************/

#include "mem/ruby/network/garnet2.0/NetworkInterface.hh"

#include <cassert>
#include <cmath>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet2.0/Credit.hh"
#include "mem/ruby/network/garnet2.0/flitBuffer.hh"
#include "mem/ruby/slicc_interface/Message.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

NetworkInterface::NetworkInterface(const Params* p)
	: ClockedObject(p), Consumer(this), m_id(p->id),
	m_virtual_networks(p->virt_nets), m_vc_per_vnet(p->vcs_per_vnet),
	m_num_vcs(m_vc_per_vnet* m_virtual_networks),
	m_deadlock_threshold(p->garnet_deadlock_threshold),
	vc_busy_counter(m_virtual_networks, 0)
{
	m_router_id = -1;
	m_num_nodes = MachineType_base_number(MachineType_NUM) / 2;					////////The number of nodes in network, equal to option num_cpus
	//printf("my num of nodes is %d", m_num_nodes);
	ddr_data_length = p->ddr_data_length;								////////////Data length sent by DDR in send_data()
	m_vc_round_robin = 0;
	m_ni_out_vcs.resize(m_num_vcs);
	m_ni_out_vcs_enqueue_time.resize(m_num_vcs);
	outCreditQueue = new flitBuffer();
	task_queue.resize(m_num_nodes - 2);		////////////////assume we have n nodes, n-2 of them are CPU and PE
													////////////////0 are HQM; 1~n/2-1 are CPU; n/2 are DDR; n/2+1~n-1 are PE
	task_queue_prepush.resize(m_num_nodes / 2 - 1);					////////////////half of  n-2 nodes are CPU, this queue is for HQM to prepush msg to CPU
	// instantiating the NI flit buffers
	for (int i = 0; i < m_num_vcs; i++) {
		m_ni_out_vcs[i] = new flitBuffer();
		m_ni_out_vcs_enqueue_time[i] = Cycles(INFINITE_);
	}

	m_vc_allocator.resize(m_virtual_networks); // 1 allocator per vnet
	for (int i = 0; i < m_virtual_networks; i++) {
		m_vc_allocator[i] = 0;
	}

	m_stall_count.resize(m_virtual_networks);
}

void
NetworkInterface::init()
{
	for (int i = 0; i < m_num_vcs; i++) {
		m_out_vc_state.push_back(new OutVcState(i, m_net_ptr));
	}
}

NetworkInterface::~NetworkInterface()
{
	deletePointers(m_out_vc_state);
	deletePointers(m_ni_out_vcs);
	delete outCreditQueue;
	delete outFlitQueue;
}

void
NetworkInterface::addInPort(NetworkLink* in_link,
	CreditLink* credit_link)
{
	inNetLink = in_link;
	in_link->setLinkConsumer(this);
	outCreditLink = credit_link;
	credit_link->setSourceQueue(outCreditQueue);
}

void
NetworkInterface::addOutPort(NetworkLink* out_link,
	CreditLink* credit_link,
	SwitchID router_id)
{
	inCreditLink = credit_link;
	credit_link->setLinkConsumer(this);

	outNetLink = out_link;
	outFlitQueue = new flitBuffer();
	out_link->setSourceQueue(outFlitQueue);

	m_router_id = router_id;
}

void
NetworkInterface::addNode(vector<MessageBuffer*>& in,
	vector<MessageBuffer*>& out)
{
	inNode_ptr = in;
	outNode_ptr = out;

	for (auto& it : in) {
		if (it != nullptr) {
			it->setConsumer(this);
		}
	}
}

void
NetworkInterface::dequeueCallback()
{
	// An output MessageBuffer has dequeued something this cycle and there
	// is now space to enqueue a stalled message. However, we cannot wake
	// on the same cycle as the dequeue. Schedule a wake at the soonest
	// possible time (next cycle).
	scheduleEventAbsolute(clockEdge(Cycles(1)));
}

void
NetworkInterface::incrementStats(flit* t_flit)
{
	int vnet = t_flit->get_vnet();

	// Latency
	m_net_ptr->increment_received_flits(vnet);
	Cycles network_delay =
		t_flit->get_dequeue_time() - t_flit->get_enqueue_time() - Cycles(1);
	Cycles src_queueing_delay = t_flit->get_src_delay();
	Cycles dest_queueing_delay = (curCycle() - t_flit->get_dequeue_time());
	Cycles queueing_delay = src_queueing_delay + dest_queueing_delay;

	m_net_ptr->increment_flit_network_latency(network_delay, vnet);
	m_net_ptr->increment_flit_queueing_latency(queueing_delay, vnet);

	if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
		m_net_ptr->increment_received_packets(vnet);
		m_net_ptr->increment_packet_network_latency(network_delay, vnet);
		m_net_ptr->increment_packet_queueing_latency(queueing_delay, vnet);
	}

	// Hops
	m_net_ptr->increment_total_hops(t_flit->get_route().hops_traversed);
}

/*
 * The NI wakeup checks whether there are any ready messages in the protocol
 * buffer. If yes, it picks that up, flitisizes it into a number of flits and
 * puts it into an output buffer and schedules the output link. On a wakeup
 * it also checks whether there are flits in the input link. If yes, it picks
 * them up and if the flit is a tail, the NI inserts the corresponding message
 * into the protocol buffer. It also checks for credits being sent by the
 * downstream router.
 */




 //Receive incoming flit, flit should be on vnet0 or 1 and only have 1 flit
void
NetworkInterface::add_task_HQM()
{
	//bool messageEnqueuedThisCycle = checkStallQueue();
	if (inNetLink->isReady(curCycle())) {
		flit* t_flit = inNetLink->consumeLink();
		int vnet = t_flit->get_vnet();
		RouteInfo route = t_flit->get_route();			
		assert((vnet == 0 || vnet == 1) && route.real_dest != m_id && route.real_dest != 3 * m_num_nodes / 2);//////In CPU & PE we set flit to HQM only in vnet0 and 1, and real dest must be other core
		t_flit->set_dequeue_time(curCycle());			
		/******Calculate forward queue id to decide whice vector to pushback******/
		int forward_queue_id;															
		if (route.real_dest > m_num_nodes && route.real_dest < 3 * m_num_nodes / 2)
			forward_queue_id = route.real_dest - (m_num_nodes + 1);
		else if (route.real_dest > (3 * m_num_nodes / 2) && route.real_dest < 2 * m_num_nodes)
			forward_queue_id = route.real_dest - (m_num_nodes + 2);
		//printf("flit's task_queue num is %d\n", forward_queue_id);

		// If a tail flit is received, enqueue into the protocol buffers if
		// space is available. Otherwise, exchange non-tail flits for credits.
		if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
			task_queue[forward_queue_id].push_back(t_flit);								//////////Put receiveing flits into queue
			if (forward_queue_id < (m_num_nodes / 2 - 1))                                           //////////If it is for CPU, also push to prepush queue
				task_queue_prepush[forward_queue_id].push_back(t_flit);
			RouteInfo routetest = task_queue[forward_queue_id].front()->get_route();
			//printf("HQM add_task_HQM vector %d push back successfully, vector length is %d\nthis flit's real dest is %d\n", forward_queue_id, (int)task_queue[forward_queue_id].size(), route.real_dest);
			incrementStats(t_flit);
		}
		else {
			// Non-tail flit. Send back a credit but not VC free signal.
			sendCredit(t_flit, false);

			// Update stats and delete flit pointer.
			incrementStats(t_flit);
			//printf("core %d receive flit in vnet %d\n", m_id, vnet);
			delete t_flit;
		}
	}
}


//DDR receive incoming request from PE or HQM, flit should be on vnet0 or 1 and only have 1 flit
void
NetworkInterface::receive_data_req()
{

	//bool messageEnqueuedThisCycle = checkStallQueue();
	if (inNetLink->isReady(curCycle())) {
		flit* t_flit = inNetLink->consumeLink();
		//int vnet = t_flit->get_vnet();
		RouteInfo route = t_flit->get_route();		
		assert((route.src_ni == m_num_nodes || (route.src_ni > (3 * m_num_nodes / 2) && route.src_ni < 2 * m_num_nodes)) && route.real_dest != (3 * m_num_nodes / 2));			//////in PE and HQM we ensure src_ni and real dest
		t_flit->set_dequeue_time(curCycle());								//////when it is flit to be forwarded, do not dequeue
		// If a tail flit is received, enqueue into the protocol buffers if
		// space is available. Otherwise, exchange non-tail flits for credits.
		if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
			m_data_queue.push_back(t_flit);									//////put receiveing flits into queue
			//RouteInfo routetest = m_data_queue.front()->get_route();
			//printf("DDR add_task_HQM vector  push back successfully, vector length is %d\nthis flit's real dest is %d\n", (int)m_data_queue.size(), route.real_dest);
			sendCredit(t_flit, true);
			incrementStats(t_flit);
		}
		else {
			// Non-tail flit. Send back a credit but not VC free signal.
			sendCredit(t_flit, false);

			// Update stats and delete flit pointer.
			incrementStats(t_flit);
			delete t_flit;
		}
	}
}




//HQM forward flit to real destination
void
NetworkInterface::get_task_HQM()
{
	for (int i = 0; i < m_num_nodes - 2; i++)
	{
		if (!(task_queue[i].empty()))
		{
			//printf("HQM queue %d is not empty\n", i);
			auto ptr = task_queue[i].begin();
			flit* flit_to_send = *ptr;
			MsgPtr msg_ptr = flit_to_send->get_msg_ptr()->clone();
			int vnet = flit_to_send->get_vnet();
			//printf("HQM get_task_HQM\tmy_vnet\t%d\n", vnet);
			int vc = calculateVC(vnet);
			//printf("HQM get_task_HQM\tmy_vnet is\t%d\tcalculateVC is %d\n", vnet, vc);
			if (vc != -1)
			{
				//MsgPtr msg_ptr = NULL;									//////////just use a null pointer regardless protocol problem
				RouteInfo route = flit_to_send->get_route();
				RouteInfo temproute;
				temproute.src_ni = route.src_ni;
				temproute.src_router = route.src_router;					//here is the src id in routing meaning
				temproute.dest_ni = route.real_dest;				///////////here we use receiving flit's real_dest as dest
				temproute.dest_router = m_net_ptr->get_router_id(route.real_dest);			//here is the dest id in routing meaning
				temproute.vc_choice = (temproute.dest_router >= m_router_id);	/////////////set vc_choice =1 when dest>src; if < then =0
				temproute.net_dest = route.net_dest;
				temproute.real_dest = route.real_dest;
				temproute.hops_traversed = -1; //route.hops_traversed;
				//size and netdest remain the same
				m_net_ptr->increment_injected_packets(vnet);
				//for (int i = 0; i < size; i++) {
				m_net_ptr->increment_injected_flits(vnet);

				flit* fl = new flit(0, vc, vnet, temproute, 1, msg_ptr,
					curCycle());
				fl->set_src_delay(flit_to_send->get_src_delay());
				m_ni_out_vcs[vc]->insert(fl);
				//}
				m_ni_out_vcs_enqueue_time[vc] = curCycle();
				m_out_vc_state[vc]->setState(ACTIVE_, curCycle());
				//delete flit_to_send;
				//m_tempflit.erase(ptr);

				//scheduleOutputLink();
				//checkReschedule1();

				sendCredit(flit_to_send, true);
				delete flit_to_send;
				task_queue[i].erase(ptr);
			}
		}
	}

	scheduleOutputLink();
	checkReschedule1();
}



//DDR send data to PE or CPU
void
NetworkInterface::send_data()
{
	if (!(m_data_queue.empty()))
	{
		auto ptr = m_data_queue.begin();
		flit* flit_to_send = *ptr;
		//MsgPtr msg_ptr = flit_to_send->get_msg_ptr()->clone();

		int vnet = 2;//flit_to_send->get_vnet();
		int vc = calculateVC(vnet);
		//printf("DDR send data\tmy_vnet is\t%d\tcalculateVC is %d\n", 2, vc);
		if (vc != -1)
		{
			MsgPtr msg_ptr = NULL;									/////////just use a null pointer regardless protocol problem
			RouteInfo route = flit_to_send->get_route();
			RouteInfo temproute;
			//printf("HQM get_task_HQM\t%d\tcreat temproute done\n", m_id);
			//int actual_dest = flit_to_send->get_route().real_dest;
			temproute.src_ni = m_id;
			temproute.src_router = m_net_ptr->get_router_id(m_id);					//here is the src id in routing meaning

			if (route.src_ni == m_num_nodes)														/////////if src is HQM, send data to CPU(real dest)
				temproute.dest_ni = route.real_dest;
			else temproute.dest_ni = route.src_ni;						/////////else src is PE, send data back

			temproute.dest_router = m_net_ptr->get_router_id(temproute.dest_ni);			//here is the dest id in routing meaning
			temproute.vc_choice = (temproute.dest_router >= m_router_id);	//////////set vc_choice =1 when dest>src; if < then =0; format defined in commontypes.hh line 61
			temproute.net_dest = route.net_dest;
			temproute.real_dest = route.real_dest;
			temproute.hops_traversed = -1;//route.hops_traversed;
			//size and netdest remain the same
			//printf("HQM get_task_HQM\t%d\textract route done\n", m_id);
			int num_flits = ddr_data_length;														///////////Use ddr_data_length to set our data length

			m_net_ptr->increment_injected_packets(vnet);
			for (int i = 0; i < num_flits; i++) {
				m_net_ptr->increment_injected_flits(vnet);
				//printf("core %d inject flit in vnet %d dest ni is %d\n", m_id, vnet, destID);
				flit* fl = new flit(i, vc, vnet, temproute, num_flits, msg_ptr,
					curCycle());

				fl->set_src_delay(flit_to_send->get_src_delay());
				m_ni_out_vcs[vc]->insert(fl);
			}
			m_ni_out_vcs_enqueue_time[vc] = curCycle();
			m_out_vc_state[vc]->setState(ACTIVE_, curCycle());

			//sendCredit(flit_to_send, true);
			delete flit_to_send;
			m_data_queue.erase(ptr);
		}
		scheduleOutputLink();
		checkReschedule1();
	}
	else {
		//printf("DDR send data m_data_queue is empty\n");
		scheduleOutputLink();
		checkReschedule1();
	}
}

//PE send data request to DDR
void
NetworkInterface::send_data_req()
{
	if (!(task_queue_PE.empty()))
	{
		auto ptr = task_queue_PE.begin();
		flit* flit_to_send = *ptr;
		//MsgPtr msg_ptr = flit_to_send->get_msg_ptr()->clone();////////get is a pointer

		int vnet = flit_to_send->get_vnet();
		int vc = calculateVC(vnet);
		//printf("PE send_data_req\tmy_vnet is\t%d\tcalculateVC is %d\n", vnet, vc);
		if (vc != -1)
		{
			MsgPtr msg_ptr = NULL;				////////just use a null pointer regardless protocol problem
			RouteInfo route = flit_to_send->get_route();
			RouteInfo temproute;
			temproute.src_ni = m_id;
			temproute.src_router = m_net_ptr->get_router_id(m_id);					//here is the src id in routing meaning
			temproute.dest_ni = (3 * m_num_nodes / 2);				///////////here we use DDR as dest
			temproute.dest_router = m_net_ptr->get_router_id(temproute.dest_ni);			//here is the dest id in routing meaning
			temproute.vc_choice = (temproute.dest_router >= m_router_id);	///////////set vc_choice =1 when dest>src; if < then =0
			temproute.net_dest = route.net_dest;
			temproute.real_dest = route.real_dest;
			temproute.hops_traversed = -1;// route.hops_traversed;
			//size and netdest remain the same
			//printf("HQM get_task_HQM\t%d\textract route done\n", m_id);
			m_net_ptr->increment_injected_packets(vnet);
			//for (int i = 0; i < size; i++) {
			m_net_ptr->increment_injected_flits(vnet);
			flit* fl = new flit(0, vc, vnet, temproute, 1, msg_ptr,
				curCycle());
			fl->set_src_delay(flit_to_send->get_src_delay());
			m_ni_out_vcs[vc]->insert(fl);
			//}
			m_ni_out_vcs_enqueue_time[vc] = curCycle();
			m_out_vc_state[vc]->setState(ACTIVE_, curCycle());
			//delete flit_to_send;
			//m_tempflit.erase(ptr);

			sendCredit(flit_to_send, true);
			delete flit_to_send;
			task_queue_PE.erase(ptr);
		}
		scheduleOutputLink();
		checkReschedule();
	}
	else {
		//printf("PE  task_queue_PE is empty\n");
		scheduleOutputLink();
		checkReschedule();
	}
}



//HQM send prefetch msg to DDR
void
NetworkInterface::prepush_data()
{
	//if (!(task_queue_prepush[0].empty() && task_queue_prepush[1].empty() && task_queue_prepush[2].empty()))
	//{
	for (int i = 0; i < (m_num_nodes / 2 - 1); i++)
	{
		if (!(task_queue_prepush[i].empty()))
		{
			auto ptr = task_queue_prepush[i].begin();
			flit* flit_to_send = *ptr;
			//MsgPtr msg_ptr = flit_to_send->get_msg_ptr()->clone();

			int vnet = flit_to_send->get_vnet();
			int vc = calculateVC(vnet);
			//printf("HQM prepush_data\tmy_vnet is\t%d\tcalculateVC is %d\n", vnet, vc);
			if (vc != -1)
			{
				MsgPtr msg_ptr = NULL;									/////////just use a null pointer regardless protocol problem
				RouteInfo route = flit_to_send->get_route();
				RouteInfo temproute;
				//printf("HQM get_task_HQM\t%d\tcreat temproute done\n", m_id);
				//int actual_dest = flit_to_send->get_route().real_dest;
				temproute.src_ni = m_id;									/////////later in DDR we use src_ni to judge if it is from HQM or PE
				temproute.src_router = m_router_id;					//here is the src id in routing meaning
				temproute.dest_ni = (3 * m_num_nodes / 2);				///////////here we use DDR as dest
				temproute.dest_router = m_net_ptr->get_router_id(temproute.dest_ni);			//here is the dest id in routing meaning
				temproute.vc_choice = (temproute.dest_router >= m_router_id);	/////////set vc_choice = 1 when dest>src; if < then = 0
				temproute.net_dest = route.net_dest;
				temproute.real_dest = route.real_dest;
				temproute.hops_traversed = -1;// route.hops_traversed;
				//size and netdest remain the same
				//printf("HQM get_task_HQM\t%d\textract route done\n", m_id);
				m_net_ptr->increment_injected_packets(vnet);
				//for (int i = 0; i < size; i++) {
				m_net_ptr->increment_injected_flits(vnet);
				flit* fl = new flit(0, vc, vnet, temproute, 1, msg_ptr,
					curCycle());

				fl->set_src_delay(flit_to_send->get_src_delay());
				m_ni_out_vcs[vc]->insert(fl);
				//}
				m_ni_out_vcs_enqueue_time[vc] = curCycle();
				m_out_vc_state[vc]->setState(ACTIVE_, curCycle());
				//sendCredit(flit_to_send, true);
				//delete flit_to_send;
				task_queue_prepush[i].erase(ptr);
			}
		}
	}
	//scheduleOutputLink();
	//checkReschedule1();
}


void
NetworkInterface::wakeup()
{
	DPRINTF(RubyNetwork, "Network Interface %d connected to router %d "
		"woke up at time: %lld\n", m_id, m_router_id, curCycle());
	//Tick curTime = clockEdge();
	/********************************************this is HQM NI***************************************************/
	if (m_id == m_num_nodes)					//////////HQMid is HQM's NI id we set it in network
	{
		//printf("HQM NI  %d  wake up\n", m_id);
		//for (int i = 0; i < (m_num_nodes - 2); i++)
		//	printf("wake up HQM check vector[%d]'s length is %d\n", i, (int)task_queue[i].size());
		add_task_HQM();
		//for (int i = 0; i < (m_num_nodes - 2); i++)
			//printf("after add_task_HQM HQM check vector[%d]'s length is %d\n", i, (int)task_queue[i].size());
		prepush_data();
		get_task_HQM();
		/****************** Check the incoming credit link *******/

		if (inCreditLink->isReady(curCycle())) {
			Credit* t_credit = (Credit*)inCreditLink->consumeLink();
			m_out_vc_state[t_credit->get_vc()]->increment_credit();
			if (t_credit->is_free_signal()) {
				m_out_vc_state[t_credit->get_vc()]->setState(IDLE_, curCycle());
			}
			delete t_credit;
		}

		// It is possible to enqueue multiple outgoing credit flits if a message
		// was unstalled in the same cycle as a new message arrives. In this
		// case, we should schedule another wakeup to ensure the credit is sent
		// back.
		if (outCreditQueue->getSize() > 0) {
			outCreditLink->scheduleEventAbsolute(clockEdge(Cycles(1)));
		}
	}
	/***********************************************this is DDR NI***************************************************/
	else if (m_id == (3 * m_num_nodes / 2))
	{
		//printf("wake up mem check vector's length is %d\n", (int)m_data_queue.size());
		receive_data_req();
		//printf("after receive_data_req mem check vector's length is %d\n", (int)m_data_queue.size());
		send_data();
		/****************** Check the incoming credit link *******/

		if (inCreditLink->isReady(curCycle())) {
			Credit* t_credit = (Credit*)inCreditLink->consumeLink();
			m_out_vc_state[t_credit->get_vc()]->increment_credit();
			if (t_credit->is_free_signal()) {
				m_out_vc_state[t_credit->get_vc()]->setState(IDLE_, curCycle());
			}
			delete t_credit;
		}

		// It is possible to enqueue multiple outgoing credit flits if a message
		// was unstalled in the same cycle as a new message arrives. In this
		// case, we should schedule another wakeup to ensure the credit is sent
		// back.
		if (outCreditQueue->getSize() > 0) {
			outCreditLink->scheduleEventAbsolute(clockEdge(Cycles(1)));
		}
	}
	/***********************************************this is CPU NI***************************************************/
	else if ((m_id >= 1 && m_id <= (m_num_nodes / 2 - 1)) || (m_id >= (m_num_nodes + 1) && m_id <= (3 * m_num_nodes / 2 - 1)))
	{
		//printf("CPU %d wake up\n", m_id);
		MsgPtr msg_ptr;
		Tick curTime = clockEdge();

		// Checking for messages coming from the protocol
		// can pick up a message/cycle for each virtual net
		for (int vnet = 0; vnet < inNode_ptr.size(); ++vnet) {						
			if (vnet == 2) {                                          ////////ignore msg generated in vnet2, CPU only send short msg in vnet0 and vnet1
				continue;
			}
			MessageBuffer* b = inNode_ptr[vnet];
			if (b == nullptr) {
				continue;
			}

			if (b->isReady(curTime)) { // Is there a message waiting
				msg_ptr = b->peekMsgPtr();
				if (flitisizeMessage(msg_ptr, vnet)) {
					b->dequeue(curTime);
				}
			}
		}
		scheduleOutputLink();
		checkReschedule();

		// Check if there are flits stalling a virtual channel. Track if a
		// message is enqueued to restrict ejection to one message per cycle.
		bool messageEnqueuedThisCycle = checkStallQueue();
		/*********** Check the incoming flit link **********/
		if (inNetLink->isReady(curCycle())) {
			flit* t_flit = inNetLink->consumeLink();
			RouteInfo t_route = t_flit->get_route();	///////////////////////////////////used for extract src_ni
			int vnet = t_flit->get_vnet();
			if (t_route.src_ni == (3 * m_num_nodes / 2))					///////////////////if src is DDR, it is data, collect infomation and delete
			{
				t_flit->set_dequeue_time(curCycle());
				if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
					sendCredit(t_flit, true);
					incrementStats(t_flit);
					delete t_flit;									
				}
				else {
					sendCredit(t_flit, false);
					// Update stats and delete flit pointer.
					incrementStats(t_flit);
					delete t_flit;
				}
			}
			else
			{
				t_flit->set_dequeue_time(curCycle());
				//printf("CPU NI  %d  receive a flit in vnet %d\n", m_id, vnet);

				// If a tail flit is received, enqueue into the protocol buffers if
				// space is available. Otherwise, exchange non-tail flits for credits.
				if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
					if (!messageEnqueuedThisCycle &&
						outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
						// Space is available. Enqueue to protocol buffer.
						outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
							cyclesToTicks(Cycles(1)));
						//printf("CPU NI  %d  receive tail flit successfully\n", m_id);
						// Simply send a credit back since we are not buffering
						// this flit in the NI
						sendCredit(t_flit, true);

						// Update stats and delete flit pointer
						incrementStats(t_flit);
						//printf("core %d receive flit in vnet %d\n", m_id, vnet);
						delete t_flit;
					}
					else {
						// No space available- Place tail flit in stall queue and set
						// up a callback for when protocol buffer is dequeued. Stat
						// update and flit pointer deletion will occur upon unstall.
						m_stall_queue.push_back(t_flit);
						m_stall_count[vnet]++;

						auto cb = std::bind(&NetworkInterface::dequeueCallback, this);
						outNode_ptr[vnet]->registerDequeueCallback(cb);
					}
				}
				else {
					// Non-tail flit. Send back a credit but not VC free signal.
					sendCredit(t_flit, false);

					// Update stats and delete flit pointer.
					incrementStats(t_flit);
					//printf("core %d receive flit in vnet %d\n", m_id, vnet);
					delete t_flit;
				}
			}

		}

		/****************** Check the incoming credit link *******/

		if (inCreditLink->isReady(curCycle())) {
			Credit* t_credit = (Credit*)inCreditLink->consumeLink();
			m_out_vc_state[t_credit->get_vc()]->increment_credit();
			if (t_credit->is_free_signal()) {
				m_out_vc_state[t_credit->get_vc()]->setState(IDLE_, curCycle());
			}
			delete t_credit;
		}

		// It is possible to enqueue multiple outgoing credit flits if a message
		// was unstalled in the same cycle as a new message arrives. In this
		// case, we should schedule another wakeup to ensure the credit is sent
		// back.
		if (outCreditQueue->getSize() > 0) {
			outCreditLink->scheduleEventAbsolute(clockEdge(Cycles(1)));
		}
	}
	/***********************************************this is PE NI***************************************************/
	else if ((m_id >= (m_num_nodes / 2 + 1) || m_id <= (m_num_nodes - 1)) || (m_id >= (3 * m_num_nodes / 2 + 1) || m_id <= (2 * m_num_nodes - 1)))
	{
		MsgPtr msg_ptr;
		//printf("PE %d wake up\n", m_id);

		Tick curTime = clockEdge();

		// Checking for messages coming from the protocol
		// can pick up a message/cycle for each virtual net
		for (int vnet = 0; vnet < inNode_ptr.size(); ++vnet) {				

			if (vnet == 2) {                                             ////////ignore msg generated in vnet2, PE only send short msg in vnet0 and vnet1
				continue;
			}

			MessageBuffer* b = inNode_ptr[vnet];
			if (b == nullptr) {
				continue;
			}

			if (b->isReady(curTime)) { // Is there a message waiting
				msg_ptr = b->peekMsgPtr();
				if (flitisizeMessage(msg_ptr, vnet)) {
					b->dequeue(curTime);
				}
			}
		}

		scheduleOutputLink();
		checkReschedule();

		// Check if there are flits stalling a virtual channel. Track if a
		// message is enqueued to restrict ejection to one message per cycle.
		bool messageEnqueuedThisCycle = checkStallQueue();

		/*********** Check the incoming flit link **********/
		if (inNetLink->isReady(curCycle())) {
			flit* t_flit = inNetLink->consumeLink();
			RouteInfo t_route = t_flit->get_route();
			int vnet = t_flit->get_vnet();
			if (t_route.src_ni == (3 * m_num_nodes / 2))							//////////if src is DDR, it is data, collect infomation and delete
			{
				t_flit->set_dequeue_time(curCycle());
				if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
					sendCredit(t_flit, true);
					incrementStats(t_flit);
					delete t_flit;	
				}
				else {
					sendCredit(t_flit, false);
					// Update stats and delete flit pointer.
					incrementStats(t_flit);
					delete t_flit;
				}
			}

			else if (vnet == 0 || vnet == 1)
			{
				task_queue_PE.push_back(t_flit);								/////////////////////if it is normal msg forwarded by HQM, we push into queue and forward it to DDR
				//int vnet = t_flit->get_vnet();
				t_flit->set_dequeue_time(curCycle());
				//printf("PE NI  %d  receive a flit in vnet %d\n", m_id, vnet);

				// If a tail flit is received, enqueue into the protocol buffers if
				// space is available. Otherwise, exchange non-tail flits for credits.
				if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
					if (!messageEnqueuedThisCycle &&
						outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
						// Space is available. Enqueue to protocol buffer.
						outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
							cyclesToTicks(Cycles(1)));
						//printf("PE NI  %d  receive tail flit successfully\n", m_id);
						// Simply send a credit back since we are not buffering
						// this flit in the NI
						//sendCredit(t_flit, true);

						// Update stats and delete flit pointer
						incrementStats(t_flit);
					}
					else {
						// No space available- Place tail flit in stall queue and set
						// up a callback for when protocol buffer is dequeued. Stat
						// update and flit pointer deletion will occur upon unstall.
						m_stall_queue.push_back(t_flit);
						m_stall_count[vnet]++;

						auto cb = std::bind(&NetworkInterface::dequeueCallback, this);
						outNode_ptr[vnet]->registerDequeueCallback(cb);
					}
				}
				else {
					// Non-tail flit. Send back a credit but not VC free signal.
					sendCredit(t_flit, false);

					// Update stats and delete flit pointer.
					incrementStats(t_flit);
					delete t_flit;
				}
			}
			else
			{
				t_flit->set_dequeue_time(curCycle());
				//printf("PE NI  %d  receive a flit in vnet %d\n", m_id, vnet);

				// If a tail flit is received, enqueue into the protocol buffers if
				// space is available. Otherwise, exchange non-tail flits for credits.
				if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
					if (!messageEnqueuedThisCycle &&
						outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
						// Space is available. Enqueue to protocol buffer.
						outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
							cyclesToTicks(Cycles(1)));
						// Simply send a credit back since we are not buffering
						// this flit in the NI
						sendCredit(t_flit, true);

						// Update stats and delete flit pointer
						incrementStats(t_flit);
						delete t_flit;
					}
					else {
						// No space available- Place tail flit in stall queue and set
						// up a callback for when protocol buffer is dequeued. Stat
						// update and flit pointer deletion will occur upon unstall.
						m_stall_queue.push_back(t_flit);
						m_stall_count[vnet]++;

						auto cb = std::bind(&NetworkInterface::dequeueCallback, this);
						outNode_ptr[vnet]->registerDequeueCallback(cb);
					}
				}
				else {
					// Non-tail flit. Send back a credit but not VC free signal.
					sendCredit(t_flit, false);

					// Update stats and delete flit pointer.
					incrementStats(t_flit);
					delete t_flit;
				}
			}
		}
		///////After everything above is done, send req to DDR
		send_data_req();

		/****************** Check the incoming credit link *******/

		if (inCreditLink->isReady(curCycle())) {
			Credit* t_credit = (Credit*)inCreditLink->consumeLink();
			m_out_vc_state[t_credit->get_vc()]->increment_credit();
			if (t_credit->is_free_signal()) {
				m_out_vc_state[t_credit->get_vc()]->setState(IDLE_, curCycle());
			}
			delete t_credit;
		}


		// It is possible to enqueue multiple outgoing credit flits if a message
		// was unstalled in the same cycle as a new message arrives. In this
		// case, we should schedule another wakeup to ensure the credit is sent
		// back.
		if (outCreditQueue->getSize() > 0) {
			outCreditLink->scheduleEventAbsolute(clockEdge(Cycles(1)));
		}

	}
	/***********************************************this is HQM & DDR's cache NI, disabled***************************************************/
	else if (m_id == 0 || m_id == (m_num_nodes / 2))
	{
		//printf("HQM & DDR cache %d wake up\n", m_id);
	}
}

void
NetworkInterface::sendCredit(flit* t_flit, bool is_free)
{
	Credit* credit_flit = new Credit(t_flit->get_vc(), is_free, curCycle());
	outCreditQueue->insert(credit_flit);
}

bool
NetworkInterface::checkStallQueue()
{
	bool messageEnqueuedThisCycle = false;
	Tick curTime = clockEdge();

	if (!m_stall_queue.empty()) {
		for (auto stallIter = m_stall_queue.begin();
			stallIter != m_stall_queue.end(); ) {
			flit* stallFlit = *stallIter;
			int vnet = stallFlit->get_vnet();

			// If we can now eject to the protocol buffer, send back credits
			if (outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
				outNode_ptr[vnet]->enqueue(stallFlit->get_msg_ptr(), curTime,
					cyclesToTicks(Cycles(1)));

				// Send back a credit with free signal now that the VC is no
				// longer stalled.
				sendCredit(stallFlit, true);

				// Update Stats
				incrementStats(stallFlit);

				// Flit can now safely be deleted and removed from stall queue
				delete stallFlit;
				m_stall_queue.erase(stallIter);
				m_stall_count[vnet]--;

				// If there are no more stalled messages for this vnet, the
				// callback on it's MessageBuffer is not needed.
				if (m_stall_count[vnet] == 0)
					outNode_ptr[vnet]->unregisterDequeueCallback();

				messageEnqueuedThisCycle = true;
				break;
			}
			else {
				++stallIter;
			}
		}
	}

	return messageEnqueuedThisCycle;
}

// Embed the protocol message into flits
bool
NetworkInterface::flitisizeMessage(MsgPtr msg_ptr, int vnet)
{
	Message* net_msg_ptr = msg_ptr.get();
	NetDest net_msg_dest = net_msg_ptr->getDestination();

	// gets all the destinations associated with this message.
	vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

	// Number of flits is dependent on the link bandwidth available.
	// This is expressed in terms of bytes/cycle or the flit size
	int num_flits = (int)ceil((double)m_net_ptr->MessageSizeType_to_int(
		net_msg_ptr->getMessageSize()) / m_net_ptr->getNiFlitSize());

	// loop to convert all multicast messages into unicast messages
	for (int ctr = 0; ctr < dest_nodes.size(); ctr++) {

		// this will return a free output virtual channel
		int vc = calculateVC(vnet);

		if (vc == -1) {
			return false;
		}
		MsgPtr new_msg_ptr = msg_ptr->clone();
		NodeID destID = dest_nodes[ctr];

		Message* new_net_msg_ptr = new_msg_ptr.get();
		if (dest_nodes.size() > 1) {
			NetDest personal_dest;
			for (int m = 0; m < (int)MachineType_NUM; m++) {
				if ((destID >= MachineType_base_number((MachineType)m)) &&
					destID < MachineType_base_number((MachineType)(m + 1))) {
					// calculating the NetDest associated with this destID
					personal_dest.clear();
					personal_dest.add((MachineID) {
						(MachineType)m, (destID -
							MachineType_base_number((MachineType)m))
					});
					new_net_msg_ptr->getDestination() = personal_dest;
					break;
				}
			}
			net_msg_dest.removeNetDest(personal_dest);
			// removing the destination from the original message to reflect
			// that a message with this particular destination has been
			// flitisized and an output vc is acquired
			net_msg_ptr->getDestination().removeNetDest(personal_dest);
		}

		// Embed Route into the flits
		// NetDest format is used by the routing table
		// Custom routing algorithms just need destID
		RouteInfo route;
		route.vnet = vnet;
		route.net_dest = new_net_msg_ptr->getDestination();
		route.src_ni = m_id;
		route.src_router = m_router_id;					//here is the src id in routing meaning
		if (destID == m_num_nodes || destID == (3 * m_num_nodes / 2))
		{
			continue;																    /////////if dest is HQM or DDR, do not send this message.
		}


		if (vnet == 0 || vnet == 1)									///////if vnet in 0 or 1, set dest as HQM
		{
			route.dest_ni = m_num_nodes;
			route.dest_router = m_net_ptr->get_router_id(route.dest_ni);
			route.real_dest = destID;
		}
		assert(vnet == 0 || vnet == 1);
		//else
		//{
		//	route.dest_ni = destID;
		//	route.dest_router = m_net_ptr->get_router_id(destID);			//here is the dest id in routing meaning
		//	route.real_dest = destID;										////////if not control type, real_dest equals destID
		//}

		route.vc_choice = (route.dest_router >= route.src_router);	//////////set vc_choice =1 when dest>src; if < then =0; format defined in commontypes.hh line 61

		// initialize hops_traversed to -1
		// so that the first router increments it to 0
		route.hops_traversed = -1;

		m_net_ptr->increment_injected_packets(vnet);
		for (int i = 0; i < num_flits; i++) {
			m_net_ptr->increment_injected_flits(vnet);
			flit* fl = new flit(i, vc, vnet, route, num_flits, new_msg_ptr,
				curCycle());

			fl->set_src_delay(curCycle() - ticksToCycles(msg_ptr->getTime()));
			m_ni_out_vcs[vc]->insert(fl);
		}

		m_ni_out_vcs_enqueue_time[vc] = curCycle();
		m_out_vc_state[vc]->setState(ACTIVE_, curCycle());
	}
	return true;
}

// Looking for a free output vc
int
NetworkInterface::calculateVC(int vnet)
{
	for (int i = 0; i < m_vc_per_vnet; i++) {
		int delta = m_vc_allocator[vnet];
		m_vc_allocator[vnet]++;
		if (m_vc_allocator[vnet] == m_vc_per_vnet)
			m_vc_allocator[vnet] = 0;

		if (m_out_vc_state[(vnet * m_vc_per_vnet) + delta]->isInState(
			IDLE_, curCycle())) {
			vc_busy_counter[vnet] = 0;
			return ((vnet * m_vc_per_vnet) + delta);
		}
	}

	vc_busy_counter[vnet] += 1;
	//panic_if(vc_busy_counter[vnet] > m_deadlock_threshold,
	//	"%s: Possible network deadlock in vnet: %d at time: %llu \n",
	//	name(), vnet, curTick());

	return -1;
}


/** This function looks at the NI buffers
 *  if some buffer has flits which are ready to traverse the link in the next
 *  cycle, and the downstream output vc associated with this flit has buffers
 *  left, the link is scheduled for the next cycle
 */

void
NetworkInterface::scheduleOutputLink()
{
	int vc = m_vc_round_robin;

	for (int i = 0; i < m_num_vcs; i++) {
		vc++;
		if (vc == m_num_vcs)
			vc = 0;

		// model buffer backpressure
		if (m_ni_out_vcs[vc]->isReady(curCycle()) &&
			m_out_vc_state[vc]->has_credit()) {

			bool is_candidate_vc = true;
			int t_vnet = get_vnet(vc);
			int vc_base = t_vnet * m_vc_per_vnet;

			if (m_net_ptr->isVNetOrdered(t_vnet)) {
				for (int vc_offset = 0; vc_offset < m_vc_per_vnet;
					vc_offset++) {
					int t_vc = vc_base + vc_offset;
					if (m_ni_out_vcs[t_vc]->isReady(curCycle())) {
						if (m_ni_out_vcs_enqueue_time[t_vc] <
							m_ni_out_vcs_enqueue_time[vc]) {
							is_candidate_vc = false;
							break;
						}
					}
				}
			}
			if (!is_candidate_vc)
				continue;

			m_vc_round_robin = vc;

			m_out_vc_state[vc]->decrement_credit();
			// Just removing the flit
			flit* t_flit = m_ni_out_vcs[vc]->getTopFlit();
			t_flit->set_time(curCycle() + Cycles(1));
			outFlitQueue->insert(t_flit);
			// schedule the out link
			outNetLink->scheduleEventAbsolute(clockEdge(Cycles(1)));

			if (t_flit->get_type() == TAIL_ ||
				t_flit->get_type() == HEAD_TAIL_) {
				m_ni_out_vcs_enqueue_time[vc] = Cycles(INFINITE_);
			}
			return;
		}
	}
}

int
NetworkInterface::get_vnet(int vc)
{
	for (int i = 0; i < m_virtual_networks; i++) {
		if (vc >= (i * m_vc_per_vnet) && vc < ((i + 1) * m_vc_per_vnet)) {
			return i;
		}
	}
	fatal("Could not determine vc");
}


// Wakeup the NI in the next cycle if there are waiting
// messages in the protocol buffer, or waiting flits in the
// output VC buffer
void
NetworkInterface::checkReschedule()
{
	for (const auto& it : inNode_ptr) {
		if (it == nullptr) {
			continue;
		}

		while (it->isReady(clockEdge())) { // Is there a message waiting
			scheduleEvent(Cycles(1));
			return;
		}
	}

	for (int vc = 0; vc < m_num_vcs; vc++) {
		if (m_ni_out_vcs[vc]->isReady(curCycle() + Cycles(1))) {
			scheduleEvent(Cycles(1));
			return;
		}
	}
}

void
NetworkInterface::checkReschedule1()			/////////////This is for HQM and DDR, no need to check message, only check vc buffer
{
	for (int vc = 0; vc < m_num_vcs; vc++) {
		if (m_ni_out_vcs[vc]->isReady(curCycle() + Cycles(1))) {
			scheduleEvent(Cycles(1));
			return;
		}
	}
}

void
NetworkInterface::print(std::ostream& out) const
{
	out << "[Network Interface]";
}

uint32_t
NetworkInterface::functionalWrite(Packet* pkt)
{
	uint32_t num_functional_writes = 0;
	for (unsigned int i = 0; i < m_num_vcs; ++i) {
		num_functional_writes += m_ni_out_vcs[i]->functionalWrite(pkt);
	}

	num_functional_writes += outFlitQueue->functionalWrite(pkt);
	return num_functional_writes;
}

NetworkInterface*
GarnetNetworkInterfaceParams::create()
{
	return new NetworkInterface(this);
}