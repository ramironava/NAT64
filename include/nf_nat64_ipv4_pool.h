/*
 * NAT64 - Network Address Translator IPv6 to IPv4
 *
 * Authors:
 *    Representative NIC-Mx
 *	Ing. Gustavo Lozano <glozano@nic.mx>
 *	Ing. Jorge Cano
 *
 *	Representative ITESM
 *	Dr. Juan Arturo Nolazco	<jnolazco@itesm.mx>
 *	Ing. Martha Sordia <msordia@itesm.mx>
 *
 *	Students ITESM
 *	Juan Antonio Osorio <jaosorior@gmail.com>
 *	Luis Fernando Hinojosa <lf.hinojosa@gmail.com>
 *	David Valenzuela <david.valenzuela.88@gmail.com>
 *	Jose Vicente Ramirez <pepermz@gmail.com>
 *	Mario Gerardo Trevinho <mario_tc88@hotmail.com>
 *	Roberto Aceves <roberto.aceves@gmail.com>
 *	Miguel Alejandro González <maggonzz@gmail.com>
 *	Ramiro Nava <ramironava@gmail.com>
 *	Adrian González <bernardogzzf@gmail.com>
 *	Manuel Aude <dormam@gmail.com>
 *	Gabriel Chavez <gabrielchavez02@gmail.com>
 *	Alan Villela López <avillop@gmail.com>
 *	  
 *	  The rest of us, I propose include our names and order all alphabetically.
 *
 * Authors of the ip_data, checksum_adjust, checksum_remove, checksum_add
 * checksum_change, adjust_checksum_ipv6_to_ipv4, nat64_output_ipv4, 
 * adjust_checksum_ipv4_to_ipv6, nat64_xlate_ipv6_to_ipv4, nat64_alloc_skb,
 * nat64_xlate_ipv4_to_ipv6 functions that belong to the Ecdysis project:
 *	Jean-Philippe Dionne <jean-philippe.dionne@viagenie.ca>
 *	Simon Perreault <simon.perreault@viagenie.ca>
 *	Marc Blanchet <marc.blanchet@viagenie.ca>
 *
 *	Ecdysis <http://ecdysis.viagenie.ca/>
 *
 * The previous functions are found in the nf_nat64_main.c file of Ecdysis's 
 * NAT64 implementation.
 *
 * Please note: 
 * The function nat64_output_ipv4 was renamed as nat64_send_packet_ipv4 
 * under the kernel version that is inferior to 3.0 in this 
 * implementation. The function nat64_send_packet_ipv6 for both
 * kernel versions were based on this function.
 *
 * The functions nat64_xlate_ipv6_to_ipv4 and nat64_xlate_ipv4_to_ipv6 were
 * used as a point of reference to implement nat64_get_skb_from6to4 and
 * nat64_get_skb_from4to6, respectively. Furthermore, nat64_alloc_skb was
 * also used as a point of reference to implement nat64_get_skb.
 * 
 * Author of the nat64_extract_ipv4, nat64_allocate_hash, tcp_timeout_fsm,
 * tcp4_fsm, tcp6_fsm, bib_allocate_local4_port, bib_ipv6_lookup, bib_ipv4_lookup,
 * bib_create, bib_session_create, session_ipv4_lookup, session_renew,
 * session_create, clean_expired_sessions functions, nat64_ipv6_input:
 *	Julius Kriukas <julius.kriukas@gmail.com>
 * 
 * 	Linux NAT64 <http://ipv6.lt/nat64_en.php>
 *
 * The previous functions are found in the nat64_session.c and nat64_core.c
 * files of Julius Kriukas's Linux NAT64 implementation. Furthermore, these
 * functions used global variables which were added (with a comment indicating
 * their origin) in our xt_nat64.c file. The majority of these functions can 
 * be found in our nf_nat64_filtering_and_updating.h file. Not all of them are 
 * being used in this release version but are planned to be used in the future.
 * This is the case of the tcp4_fsm, tcp6_fsm, tcp_timeout_fsm and 
 * clean_expired_sessions functions and some of the global variables they use.
 * Part of our nat64_filtering_and_updating function was based on Julius's 
 * implementation of his nat64_ipv6_input function.
 *
 * NAT64 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NAT64 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with NAT64.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef _NF_NAT64_IPV4_POOL_H
#define _NF_NAT64_IPV4_POOL_H

//~ #define FIRST_ADDRESS "192.168.2.1"
//~ #define LAST_ADDRESS "192.168.2.20"
//~ #define FIRST_PORT 1024
//~ #define LAST_PORT 65534

#include "xt_nat64_module_conf.h"

struct transport_addr_struct {
    char *address;
    __be16 port;
    struct list_head list;
};

__be32 next_udp_address; // FIXME: Rob. Think of change this datatype by: struct in_addr
__be32 next_tcp_address;
__be32 last_address;
int next_udp_port;
int next_tcp_port;
int first_udp_port;
int first_tcp_port;
int last_udp_port;
int last_tcp_port;

struct list_head free_udp_transport_addr;
struct list_head free_tcp_transport_addr;

__be32 swap_endians(__be32 be)
{
    __be32 le = ((be & 0xFF) << 24)
                      | ((be & 0xFF00) << 8)
                      | ((be >> 8) & 0xFF00)
                      | (be >> 24);
    return le;
}

char *ip_address_to_string(__be32 ip)
{
    char *result = (char *)kmalloc((sizeof(char))*INET_ADDRSTRLEN, GFP_ATOMIC);
    
    sprintf(result, "%d.%d.%d.%d",
            (ip      ) & 0xFF,
            (ip >>  8) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 24) & 0xFF);
    
    return result;
}

struct transport_addr_struct
	*get_transport_addr(struct list_head *head, int *next_address,
						int *next_port, int *first_port, int *last_port)
{
    // if the list is empty
    if(list_empty(head) == 1){
        // and the next address is greater than the last address, return NULL
        if(*next_address > last_address){
            return NULL;
        }
        // get the next address
        else{
            struct transport_addr_struct *new_transport_addr = (struct transport_addr_struct *)kmalloc(sizeof(struct transport_addr_struct), GFP_ATOMIC);
            
            if(new_transport_addr != NULL){
                __be32 r = swap_endians(*next_address);
                
                new_transport_addr->address = ip_address_to_string(r);
                
                new_transport_addr->port = (*next_port)++;
    
                if(*next_port > *last_port){
                    *next_port = *first_port;
                    (*next_address)++;
                }
    
                return new_transport_addr;
            
            }else{
                return NULL;
            }
        }
    }
    // is not empty
    else{
        // get the last address of the list
        struct list_head *prev = head->prev;
        struct transport_addr_struct *transport_addr = list_entry(prev, struct transport_addr_struct, list);
        list_del(prev);
        return transport_addr;
    }
}

struct transport_addr_struct *get_udp_transport_addr()
{
  return get_transport_addr(&free_udp_transport_addr, &next_udp_address,
							&next_udp_port, &first_udp_port, &last_udp_port);
}

struct transport_addr_struct *get_tcp_transport_addr()
{
  return get_transport_addr(&free_tcp_transport_addr, &next_tcp_address,
							&next_tcp_port, &first_tcp_port, &last_tcp_port);
}


void return_transport_addr(struct transport_addr_struct *transport_addr, struct list_head *head)
{
    INIT_LIST_HEAD(&transport_addr->list);
    list_add(&transport_addr->list, head);
}

void return_tcp_transport_addr(struct transport_addr_struct *transport_addr)
{
  return_transport_addr(transport_addr, &free_tcp_transport_addr);
}

void return_udp_transpsort_addr(struct transport_addr_struct *transport_addr)
{
  return_transport_addr(transport_addr, &free_udp_transport_addr);
}

//~ void init_pools(void)
void init_pools(struct *in_addr first_pool_address)
{
    __be32 r1,r2;
    char *add1;
    char *add2;
// ROB: me quedé adaptando esta función para que tome los valores como
// 		parámetros de entrada, en lugar de tomarlos de DEFINEs. Y así
//		permitir el cambio en los valores de la pool mediante el archivo
//		de configuración.   
    //~ in4_pton(IPV4_DEF_POOL_FIRST, -1, (u8 *)&next_udp_address, '\x0', NULL);
    //~ in4_pton(IPV4_DEF_POOL_FIRST, -1, (u8 *)&next_tcp_address, '\x0', NULL);
	next_udp_address = (*first_pool_address).s_addr;
	next_tcp_address = (*first_pool_address).s_addr;

    next_udp_address = swap_endians(next_udp_address);
    next_tcp_address = swap_endians(next_tcp_address);
    
    in4_pton(IPV4_DEF_POOL_LAST, -1, (u8 *)&last_address, '\x0', NULL);
    last_address = swap_endians(last_address);
                  
    first_tcp_port = IPV4_DEF_TCP_POOL_FIRST; // FIRST_PORT;
    first_udp_port = IPV4_DEF_UDP_POOL_FIRST;
    next_udp_port = first_udp_port;
    next_tcp_port = first_tcp_port;
    last_tcp_port = IPV4_DEF_TCP_POOL_LAST; // LAST_PORT;
	last_udp_port = IPV4_DEF_UDP_POOL_LAST;
    
    r1 = swap_endians(next_udp_address);
    r2 = swap_endians(last_address);
    
    add1 = ip_address_to_string(r1);
    add2 = ip_address_to_string(r2);
    
    INIT_LIST_HEAD(&free_udp_transport_addr);
    INIT_LIST_HEAD(&free_tcp_transport_addr);
    
    pr_debug("NAT64: First address: %s - Last address: %s\n", add1, add2);
    pr_debug("NAT64: First UDP port: %u - Last port: %u\n", first_udp_port, last_udp_port);
    pr_debug("NAT64: First TCP port: %u - Last port: %u\n", first_tcp_port, last_tcp_port);
}

 #endif
