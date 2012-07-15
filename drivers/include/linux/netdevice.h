#ifndef __NETDEVICE_H__
#define __NETDEVICE_H__

#include <vmm_host_irq.h>
#include <net/vmm_netdev.h>

#define	net_device		vmm_netdev
#define	netdev_priv		vmm_netdev_get_priv
#define	netif_rx		vmm_netif_rx
#define	netif_wake_queue	vmm_netif_wake_queue
#define	netif_stop_queue	vmm_netif_stop_queue
#define	netif_start_queue	vmm_netif_start_queue
#define	netif_carrier_off	vmm_netif_carrier_off

#define netif_msg_link(x)	0

/* Move this to irqreturn.h */
#define	IRQ_NONE		VMM_IRQ_NONE
#define	IRQ_HANDLED		VMM_IRQ_HANDLED


#endif
