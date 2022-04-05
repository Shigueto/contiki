/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/resolv.h"
#include "dev/leds.h"
#include "button-sensor.h"

#include <string.h>
#include <stdbool.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define SEND_INTERVAL		(5 * CLOCK_SECOND)
#define MAX_PAYLOAD_LEN		(1)
#define CONN_PORT     (8802)
#define MDNS (1)
#define ceiot_abs(x) x>0?x:-1*x

#define LED_TOGGLE_REQUEST (0x79)
#define LED_SET_STATE (0x7A)
#define LED_GET_STATE (0x7B)
#define LED_STATE (0x7C)

#define OP_REQUEST (0x6E)
#define OP_RESULT (0x6F)

#define OP_MULTIPLY (0x22)
#define OP_DIVIDE (0x23)
#define OP_SUM (0x24)
#define OP_SUBTRACT (0x25)

struct mathopreq {
  uint8_t opRequest;
  int32_t op1;
  uint8_t operation;
  int32_t op2;
  float fc;
}  __attribute__((packed));

struct mathopreply {
  uint8_t opResult;
  int32_t intPart;
  uint32_t fracPart;
  float fpResult;
  uint8_t crc;
}  __attribute__((packed));

static char buf[MAX_PAYLOAD_LEN];

static struct uip_udp_conn *client_conn;

#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN])
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define LED_VERBOSE 0
#define SEND_LED_TOGGLE 0

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
PROCESS(read_button_process, "Read HW buttons to trigger UDP");
AUTOSTART_PROCESSES(&resolv_process, &udp_client_process, &read_button_process);
/*---------------------------------------------------------------------------*/

static void send_response(char value) {
    char response[2] = {
                        LED_STATE,
                        value,
    };
    uip_ipaddr_copy(&client_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    client_conn->rport = UIP_UDP_BUF->destport;
    uip_udp_packet_send(client_conn, response, uip_datalen());
#if LED_VERBOSE==1
    PRINTF("Enviado LED_STATE=%d para [", value);
    PRINT6ADDR(&client_conn->ripaddr);
    PRINTF("]:%u\n", UIP_HTONS(client_conn->rport));
#endif
}

static uint8_t print_byte_array(char* bytes, int size) {
    int sum_until_13th = 0;
    uint8_t result = 0;
    printf("bytes: ");
    for (int i=0; i<size; i++) {
        printf("%d ", (int)bytes[i]);
        if (i<=12) {
            sum_until_13th += (int)bytes[i];
            result += (uint8_t)bytes[i];
        }
    }
    printf(", sum=%d, result=%d\n", sum_until_13th, (int)result);
    return result;
}

static void print_float(char* field, float f) {
    float num=ceiot_abs(f);
    printf("%s=%s%d.%d\n", field, f>0?" ":"-", (int)(num),(int)((num-((int)(num)))*1000));
}

static void print_request(struct mathopreq req) {
    printf("=== op request ===\n");
    printf("opRequest=0x%x\n", req.opRequest);
    printf("op1=%d\n", (int)req.op1);
    printf("operation=0x%x (%d)\n", req.operation, req.operation);
    printf("op2=%d\n", (int)req.op2);
    print_float("fc", req.fc);
    print_byte_array((char*)&req, sizeof(struct mathopreq));
    printf("==================\n");
}



static void print_result(struct mathopreply* reply) {
    //float num=ceiot_abs(reply->fpResult);

    printf("=== op result ===\n");
    printf("opResult=0x%x (%d)\n", reply->opResult, reply->opResult);
    printf("intPart=%d\n", (int)reply->intPart);
    printf("fracPart=%d\n", (int)reply->fracPart);
    print_float("fpResult", reply->fpResult);
    printf("crc=%d\n", reply->crc);
    uint8_t crc = print_byte_array((char*)reply, sizeof(struct mathopreply));
    printf("crc ok=%d\n", reply->crc == crc);
    printf("==================\n");
}

static void
tcpip_handler(void)
{
    static char led_state = 0;
    char *dados;

    if(uip_newdata()) {
        dados = uip_appdata;

        switch (dados[0]) {
        case LED_SET_STATE:
            dados[uip_datalen()] = '\0';
#if LED_VERBOSE==1
            printf("LED_SET_STATE recebido do servidor: %d\n", dados[1]);
#endif
            leds_off(LEDS_GREEN|LEDS_RED);
            switch (dados[1]) {
            case 0:
                break;
            case 1:
                leds_on(LEDS_GREEN);
                break;
            case 2:
                leds_on(LEDS_RED);
                break;
            case 3:
                leds_on(LEDS_GREEN|LEDS_RED);
                break;
            }
            led_state = dados[1];
            send_response(led_state);
            break;
        case LED_GET_STATE:
            dados[uip_datalen()] = '\0';
#if LED_VERBOSE==1
            printf("LED_GET_STATE recebido do servidor\n");
#endif
            send_response(led_state);
            break;
        case OP_RESULT: {
            struct mathopreply* reply = (struct mathopreply*)dados;
            print_result(reply);
            break;
        }
        }

    }
}
/*---------------------------------------------------------------------------*/
static void
timeout_handler(void)
{
    char payload = LED_TOGGLE_REQUEST;

    buf[0] = payload;
    if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
      PRINTF("Aguardando auto-configuracao de IP\n");
      return;
    }
#if LED_VERBOSE==1
    PRINTF("Enviando LED_TOGGLE_REQUEST para [");
    PRINT6ADDR(&client_conn->ripaddr);
    PRINTF("]:%u\n", UIP_HTONS(client_conn->rport));
#endif
#if SEND_LED_TOGGLE==1
    uip_udp_packet_send(client_conn, buf, sizeof(MAX_PAYLOAD_LEN));
#endif
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
#if 0 //UIP_CONF_ROUTER
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}
#endif /* UIP_CONF_ROUTER */
/*---------------------------------------------------------------------------*/

#if MDNS

static resolv_status_t
set_connection_address(uip_ipaddr_t *ipaddr)
{
#ifndef UDP_CONNECTION_ADDR
#if RESOLV_CONF_SUPPORTS_MDNS
#define UDP_CONNECTION_ADDR       contiki-udp-server.local
#elif UIP_CONF_ROUTER
#define UDP_CONNECTION_ADDR       fd00:0:0:0:0212:7404:0004:0404
#else
#define UDP_CONNECTION_ADDR       fe80:0:0:0:6466:6666:6666:6666
#endif
#endif /* !UDP_CONNECTION_ADDR */

#define _QUOTEME(x) #x
#define QUOTEME(x) _QUOTEME(x)

    resolv_status_t status = RESOLV_STATUS_ERROR;

    if(uiplib_ipaddrconv(QUOTEME(UDP_CONNECTION_ADDR), ipaddr) == 0) {
        uip_ipaddr_t *resolved_addr = NULL;
        status = resolv_lookup(QUOTEME(UDP_CONNECTION_ADDR),&resolved_addr);
        if(status == RESOLV_STATUS_UNCACHED || status == RESOLV_STATUS_EXPIRED) {
            PRINTF("Attempting to look up %s\n",QUOTEME(UDP_CONNECTION_ADDR));
            resolv_query(QUOTEME(UDP_CONNECTION_ADDR));
            status = RESOLV_STATUS_RESOLVING;
        } else if(status == RESOLV_STATUS_CACHED && resolved_addr != NULL) {
            PRINTF("Lookup of \"%s\" succeded!\n",QUOTEME(UDP_CONNECTION_ADDR));
        } else if(status == RESOLV_STATUS_RESOLVING) {
            PRINTF("Still looking up \"%s\"...\n",QUOTEME(UDP_CONNECTION_ADDR));
        } else {
            PRINTF("Lookup of \"%s\" failed. status = %d\n",QUOTEME(UDP_CONNECTION_ADDR),status);
        }
        if(resolved_addr)
            uip_ipaddr_copy(ipaddr, resolved_addr);
    } else {
        status = RESOLV_STATUS_CACHED;
    }

    return status;
}
#endif

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer et;
  uip_ipaddr_t ipaddr;

  PROCESS_BEGIN();
  PRINTF("UDP client process started\n");

#if 0 //UIP_CONF_ROUTER
  set_global_address();
#else
  etimer_set(&et, 2*CLOCK_SECOND);
  while(uip_ds6_get_global(ADDR_PREFERRED) == NULL)
  {
      PROCESS_WAIT_EVENT();
      if(etimer_expired(&et))
      {
          PRINTF("Aguardando auto-configuracao de IP\n");
          etimer_set(&et, 2*CLOCK_SECOND);
      }
  }
#endif

  print_local_addresses();

#if MDNS
  char contiki_hostname[16];
  sprintf(contiki_hostname,"node%02X%02X",linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
  resolv_set_hostname(contiki_hostname);
  PRINTF("Setting hostname to %s\n",contiki_hostname);

  static resolv_status_t status = RESOLV_STATUS_UNCACHED;
  while(status != RESOLV_STATUS_CACHED) {
      status = set_connection_address(&ipaddr);

      if(status == RESOLV_STATUS_RESOLVING) {
          //PROCESS_WAIT_EVENT_UNTIL(ev == resolv_event_found);
          PROCESS_WAIT_EVENT();
      } else if(status != RESOLV_STATUS_CACHED) {
          PRINTF("Can't get connection address.\n");
          //PROCESS_YIELD();
          PROCESS_WAIT_EVENT();
      }
  }
#else
  //configures the destination IPv6 address
  uip_ip6addr(&ipaddr, 0xfe80, 0, 0, 0, 0x215, 0x2000, 0x0002, 0x2145);
#endif
  /* new connection with remote host */
  client_conn = udp_new(&ipaddr, UIP_HTONS(CONN_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(CONN_PORT));

  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
	UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

  etimer_set(&et, SEND_INTERVAL);
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)) {
      timeout_handler();
      etimer_restart(&et);
    } else if(ev == tcpip_event) {
      tcpip_handler();
    }
  }

  PROCESS_END();
}

PROCESS_THREAD(read_button_process, ev, data)
{
    PROCESS_BEGIN();

    while(1){
        PROCESS_YIELD();

        if (ev == sensors_event) {
            if (data == &button_left_sensor || data == &button_right_sensor) {
                struct mathopreq req;
                req.opRequest = OP_REQUEST;
                req.op1 = 1;
                req.op2 = 3;
                req.operation = OP_SUM;
                req.fc = -3.14;

                uip_udp_packet_send(client_conn, (void*)&req, sizeof(struct mathopreq));
                PRINTF("Sending operation 0x%x para [", req.operation);
                PRINT6ADDR(&client_conn->ripaddr);
                PRINTF("]:%u\n", UIP_HTONS(client_conn->rport));
                print_request(req);
            }
        }
    }
    PROCESS_END();
    return 0;
}
/*---------------------------------------------------------------------------*/
