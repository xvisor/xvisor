#ifndef _EVENTS_I2C_H_
#define _EVENTS_I2C_H_

#if !defined(_TRACE_I2C_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_I2C_H

#include <linux/i2c.h>
#include <linux/tracepoint.h>

#define trace_smbus_read(adapter, addr, flags, read_write, command, protocol)
#define trace_smbus_write(adapter, addr, flags, read_write, command,	\
			  protocol, data)
#define trace_smbus_reply(adapter, addr, flags, read_write, command,	\
			  protocol, data)
#define trace_smbus_result(adapter, addr, flags, read_write, command,	\
			   protocol, res)
#define trace_i2c_read(adapter, msg, num)
#define trace_i2c_write(adapter, msg, num)
#define trace_i2c_reply(adapter, msg, num)
#define trace_i2c_result(adapter, num, ret)

#endif /* _TRACE_I2C_H */

#endif /* _EVENTS_I2C_H_ */
