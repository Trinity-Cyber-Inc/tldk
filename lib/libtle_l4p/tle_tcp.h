/*
 * Copyright (c) 2016-2017  Intel Corporation.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TLE_TCP_H_
#define _TLE_TCP_H_

#include <tle_ctx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TCP stream states
 */
enum {
	TLE_TCP_ST_CLOSED,
	TLE_TCP_ST_LISTEN,
	TLE_TCP_ST_SYN_SENT,
	TLE_TCP_ST_SYN_RCVD,
	TLE_TCP_ST_ESTABLISHED,
	TLE_TCP_ST_FIN_WAIT_1,
	TLE_TCP_ST_FIN_WAIT_2,
	TLE_TCP_ST_CLOSE_WAIT,
	TLE_TCP_ST_CLOSING,
	TLE_TCP_ST_LAST_ACK,
	TLE_TCP_ST_TIME_WAIT,
	TLE_TCP_ST_NUM
};

/**
 * User control operations for TCP stream
 */
enum {
	TLE_TCP_OP_LISTEN =    0x1,
	TLE_TCP_OP_ACCEPT =    0x2,
	TLE_TCP_OP_CONNECT =   0x4,
	TLE_TCP_OP_ESTABLISH = 0x8,
	TLE_TCP_OP_SHUTDOWN  = 0x10,
	TLE_TCP_OP_CLOSE =     0x20,
	TLE_TCP_OP_ABORT =     0x40,
};

#define TLE_TCP_OP_CLOSE_ABORT	(TLE_TCP_OP_CLOSE | TLE_TCP_OP_ABORT)

/**
 * termination/error events from remote peer
 */
enum {
	TLE_TCP_REV_FIN = 0x1,  /** FIN received from peer*/
	TLE_TCP_REV_RST = 0x2,  /** RST received from peer */
	TLE_TCP_REV_RTO = 0x4,  /** receive timed-out */
};

/*
 * flags for stream creation/establishment
 */
enum {
	/**
	 * don't put stream into internal stream table
	 * Note that tle_tcp_rx_bulk() wouldn't able to properly process
	 * packets for such stream.
	 */
	TLE_TCP_STREAM_F_PRIVATE = 0x1,
};

/**
 * TCP stream creation parameters.
 */
struct tle_tcp_stream_addr {
	struct sockaddr_storage local;  /**< stream local address. */
	struct sockaddr_storage remote; /**< stream remote address. */
};

#define	TLE_TCP_DEFAULT_RETRIES	3

struct tle_tcp_stream_cfg {
	uint8_t nb_retries;     /**< max number of retransmission attempts. */

	uint64_t udata; /**< user data to be associated with the stream. */

	/* _cb and _ev are mutually exclusive */
	struct tle_event *err_ev;      /**< error event to use.  */
	struct tle_stream_cb err_cb;   /**< error callback to use. */

	struct tle_event *recv_ev;      /**< recv event to use.  */
	struct tle_stream_cb recv_cb;   /**< recv callback to use. */

	struct tle_event *send_ev;      /**< send event to use. */
	struct tle_stream_cb send_cb;   /**< send callback to use. */
};

struct tle_tcp_stream_param {
	struct tle_tcp_stream_addr addr;
	struct tle_tcp_stream_cfg cfg;
};

/**
 * Timestamp option.
 */
union tle_tcp_tsopt {
	uint64_t raw;
	struct {
		uint32_t val;
		uint32_t ecr;
	};
};

/**
 * SYN time option values.
 */
struct tle_tcp_syn_opts {
	/* mss to use when communicating with the peer */
	uint16_t mss;
	/* peer window scaling factor */
	uint8_t  wscale;
	/* local window scaling factor, only used via tcp_establish */
	uint8_t  l_wscale;
	union tle_tcp_tsopt ts;
};

struct tle_tcp_conn_info {
	uint16_t wnd;
	uint32_t seq;
	uint32_t ack;
	struct tle_tcp_syn_opts so;
};

/**
 * TCP stream state information.
 */
struct tle_tcp_stream_state {
	/** current TCP state (one of TLE_TCP_ST_*) */
	uint16_t state;
	/** bitmask of control ops performed by user (TLE_TCP_OP_*) */
	uint16_t uop;
	/** bitmask of remote termination events (TLE_TCP_REV_*) */
	uint16_t rev;
};

/**
 * create a new stream within given TCP context.
 * @param ctx
 *   TCP context to create new stream within.
 * @param prm
 *   Parameters used to create and initialise the new stream.
 * @return
 *   Pointer to TCP stream structure that can be used in future TCP API calls,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENOFILE - max limit of open streams reached for that context
 */
struct tle_stream *
tle_tcp_stream_open(struct tle_ctx *ctx,
	const struct tle_tcp_stream_param *prm);

/**
 * close an open stream.
 * if the stream is in connected state, then:
 * - connection termination would be performed.
 * - if stream contains unsent data, then actual close will be postponed
 * till either remaining data will be TX-ed, or timeout will expire.
 * All packets that belong to that stream and remain in the device
 * TX queue will be kept for further transmission.
 * @param s
 *   Pointer to the stream to close.
 * @return
 *   zero on successful completion.
 *   - -EINVAL - invalid parameter passed to function
 *   - -EDEADLK - close was already invoked on that stream
 */
int tle_tcp_stream_close(struct tle_stream *s);

/**
 * half-close for open stream.
 * if the stream is in connected or close-wait state, then:
 * - FIN packet will be generated and stream state will be changed accordingly.
 * Note that stream will remain open till user will call actual close()
 * for that stream (even if actual connection was already terminated).
 * @param s
 *   Pointer to the stream to close.
 * @return
 *   zero on successful completion.
 *   - -EINVAL - invalid parameter passed to function
 *   - -EDEADLK - shutdown/close was already invoked on that stream
 */
int tle_tcp_stream_shutdown(struct tle_stream *s);

/**
 * abnormal stream termination.
 * if the stream is in connected state, then:
 * - abnormal connection termination would be performed.
 * - if stream contains unread data, then it will be wiped out.
 * - if stream contains unsent data, then it will be wiped out,
 *   without further attempt to TX it.
 * All packets that belong to that stream and remain in the device
 * TX queue will be kept for further transmission.
 * @param s
 *   Pointer to the stream to close.
 * @return
 *   zero on successful completion.
 *   - -EINVAL - invalid parameter passed to function
 *   - -EDEADLK - close was already invoked on that stream
 */
int tle_tcp_stream_abort(struct tle_stream *s);


/**
 * close a group of open streams.
 * if the stream is in connected state, then:
 * - connection termination would be performed.
 * - if stream contains unsent data, then actual close will be postponed
 * till either remaining data will be TX-ed, or timeout will expire.
 * All packets that belong to that stream and remain in the device
 * TX queue will be kept for father transmission.
 * @param ts
 *   An array of pointers to streams that have to be closed.
 * @param num
 *   Number of elements in the *ts* array.
 * @return
 *   number of successfully closed streams.
 *   In case of error, error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - EDEADLK - close was already invoked on that stream
 */
uint32_t
tle_tcp_stream_close_bulk(struct tle_stream *ts[], uint32_t num);

/**
 * get open stream local and remote addresses.
 * @param s
 *   Pointer to the stream.
 * @return
 *   zero on successful completion.
 *   - EINVAL - invalid parameter passed to function
 */
int
tle_tcp_stream_get_addr(const struct tle_stream *s,
	struct tle_tcp_stream_addr *addr);

/**
 * Get current TCP maximum segment size
 * @param ts
 *   Stream to retrieve MSS information from.
 * @return
 *   Maximum segment size in bytes, if successful.
 *   Negative on failure.
 */
int tle_tcp_stream_get_mss(const struct tle_stream *ts);

/**
 * Get current TCP stream state
 * @param ts
 *   Stream to retrieve state information from.
 * @return
 *   zero on successful completion.
 *   - EINVAL - invalid parameter passed to function
 */
int tle_tcp_stream_get_state(const struct tle_stream *ts,
	struct tle_tcp_stream_state *st);

/**
 * create a new stream within given TCP context.
 * Stream is put into ESTABLISHED state stragithway.
 * Connection state is recreated based on provided information.
 * @param ctx
 *   TCP context to create new stream within.
 * @param prm
 *   Parameters used to create and initialise the new stream.
 * @param ci
 *   Connection state values to recreate.
 * @param flags
 *   Combination of TLE_TCP_STREAM_F_* values.
 * @return
 *   Pointer to TCP stream structure that can be used in future TCP API calls,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENOFILE - max limit of open streams reached for that context
 */
struct tle_stream *
tle_tcp_stream_establish(struct tle_ctx *ctx,
	const struct tle_tcp_stream_param *prm,
	const struct tle_tcp_conn_info *ci, uint32_t flags);

/**
 * Client mode connect API.
 */

/**
 * Attempt to establish connection with the destination TCP endpoint.
 * Stream write event (or callback) will fire, if the connection will be
 * established successfully.
 * Note that stream in listen state or stream with already established
 * connection, can't be subject of connect() call.
 * In case of unsuccessful attempt, error event (or callback) will be
 * activated.
 * @param s
 *   Pointer to the stream.
 * @param addr
 *   Address of the destination endpoint.
 * @return
 *   zero on successful completion.
 *   - -EINVAL - invalid parameter passed to function
 */
int tle_tcp_stream_connect(struct tle_stream *s, const struct sockaddr *addr);

/*
 * Server mode connect API.
 * Basic scheme for server mode API usage:
 *
 * <stream open happens here>
 * tle_tcp_stream_listen(stream_to_listen);
 * <wait for read event/callback on that stream>
 * n = tle_tcp_accept(stream_to_listen, accepted_streams,
 * 	sizeof(accepted_streams));
 * for (i = 0, i != n; i++) {
 * 	//prepare tle_tcp_stream_cfg for newly accepted streams
 * 	...
 * }
 * k = tle_tcp_stream_update_cfg(rs, prm, n);
 * if (n != k) {
 * 	//handle error
 *	...
 * }
 */

/**
 * Set stream into the listen state (passive opener), i.e. make stream ready
 * to accept new connections.
 * Stream read event (or callback) will be activated as new SYN requests
 * will arrive.
 * Note that stream with already established (or establishing) connection
 * can't be subject of listen() call.
 * @param s
 *   Pointer to the stream.
 * @return
 *   zero on successful completion.
 *   - -EINVAL - invalid parameter passed to function
 */
int tle_tcp_stream_listen(struct tle_stream *s);

/**
 * return up to *num* streams from the queue of pending connections
 * for given TCP endpoint.
 * @param s
 *   TCP stream in listen state.
 * @param rs
 *   An array of pointers to the newily accepted streams.
 *   Each such new stream represents a new connection to the given TCP endpoint.
 *   Newly accepted stream should be in connected state and ready to use
 *   by other FE API routines (send/recv/close/etc.).
 * @param num
 *   Number of elements in the *rs* array.
 * @return
 *   number of entries filled inside *rs* array.
 */
uint16_t tle_tcp_stream_accept(struct tle_stream *s, struct tle_stream *rs[],
	uint32_t num);

/**
 * updates configuration (associated events, callbacks, stream parameters)
 * for the given streams.
 * @param ts
 *   An array of pointers to the streams to update.
 * @param prm
 *   An array of parameters to update for the given streams.
 * @param num
 *   Number of elements in the *ts* and *prm* arrays.
 * @return
 *   number of streams successfully updated.
 *   In case of error, error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 */
uint32_t tle_tcp_stream_update_cfg(struct tle_stream *ts[],
	struct tle_tcp_stream_cfg prm[], uint32_t num);

/**
 * Accept connection requests for the given stream.
 * Note that the stream has to be in listen state.
 * For each new connection a new stream will be open.
 * @param s
 *   TCP listen stream.
 * @param prm
 *   An array of *tle_tcp_accept_param* structures that
 *   contains at least *num* elements in it.
 * @param rs
 *   An array of pointers to *tle_stream* structures that
 *   must be large enough to store up to *num* pointers in it.
 * @param num
 *   Number of elements in the *prm* and *rs* arrays.
 * @return
 *   number of of entries filled inside *rs* array.
 *   In case of error, error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENFILE - no more streams are avaialble to open.
 */

/**
 * Return up to *num* mbufs that was received for given TCP stream.
 * Note that the stream has to be in connected state.
 * Data ordering is preserved.
 * For each returned mbuf:
 * data_off set to the start of the packet's TCP data
 * l2_len, l3_len, l4_len are setup properly
 * (so user can still extract L2/L3 address info if needed)
 * packet_type RTE_PTYPE_L2/L3/L4 bits are setup properly.
 * L3/L4 checksum is verified.
 * @param s
 *   TCP stream to receive packets from.
 * @param pkt
 *   An array of pointers to *rte_mbuf* structures that
 *   must be large enough to store up to *num* pointers in it.
 * @param num
 *   Number of elements in the *pkt* array.
 * @return
 *   number of of entries filled inside *pkt* array.
 */
uint16_t tle_tcp_stream_recv(struct tle_stream *s, struct rte_mbuf *pkt[],
	uint16_t num);

/**
 * Reads iovcnt buffers from the for given TCP stream.
 * Note that the stream has to be in connected state.
 * Data ordering is preserved.
 * Buffers are processed in array order.
 * This means that the function will comppletely fill iov[0]
 * before proceeding to iov[1], and so on.
 * If there is insufficient data, then not all buffers pointed to by iov
 * may be filled.
 * @param ts
 *   TCP stream to receive data from.
 * @param iov
 *   Points to an array of iovec structures.
 * @param iovcnt
 *   Number of elements in the *iov* array.
 * @return
 *   On success, number of bytes read in the stream receive buffer.
 *   In case of error, returns -1 and error code will be set in rte_errno.
 */
ssize_t tle_tcp_stream_readv(struct tle_stream *ts, const struct iovec *iov,
	int iovcnt);

/**
 * Consume and queue up to *num* packets, that will be sent eventually
 * by tle_tcp_tx_bulk().
 * Note that the stream has to be in connected state.
 * It is responsibility of that function is to determine over which TCP dev
 * given packets have to be sent out and do necessary preparations for that.
 * Based on the *dst_addr* it does route lookup, fills L2/L3/L4 headers,
 * and, if necessary, fragments packets.
 * Depending on the underlying device information, it either does
 * IP/TCP checksum calculations in SW or sets mbuf TX checksum
 * offload fields properly.
 * For each input mbuf the following conditions have to be met:
 *	- data_off point to the start of packet's TCP data.
 *	- there is enough header space to prepend L2/L3/L4 headers.
 * @param s
 *   TCP stream to send packets over.
 * @param pkt
 *   The burst of output packets that need to be send.
 * @param num
 *   Number of elements in the *pkt* array.
 * @return
 *   number of packets successfully queued in the stream send buffer.
 *   In case of error, error code can be set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EAGAIN - operation can't be perfomed right now
 *              (most likely close() was perfomed on that stream allready).
 *   - ENOTCONN - the stream is not connected.
 */
uint16_t tle_tcp_stream_send(struct tle_stream *s, struct rte_mbuf *pkt[],
	uint16_t num);

/**
 * Writes iovcnt buffers of data described by iov to the for given TCP stream.
 * Note that the stream has to be in connected state.
 * Data ordering is preserved.
 * Buffers are processed in array order.
 * This means that the function will write out the entire contents of iov[0]
 * before proceeding to iov[1], and so on.
 * If there is insufficient space in stream send buffer,
 * then not all buffers pointed to by iov may be written out.
 * @param ts
 *   TCP stream to send data to.
 * @param iov
 *   Points to an array of iovec structures.
 * @param iovcnt
 *   Number of elements in the *iov* array.
 * @return
 *   On success, number of bytes written to the stream send buffer.
 *   In case of error, returns -1 and error code will be set in rte_errno.
 *   - EAGAIN - operation can't be perfomed right now
 *              (most likely close() was perfomed on that stream allready).
 *   - ENOTCONN - the stream is not connected.
 *   - ENOMEM - not enough internal buffer (mbuf) to store user provided data.
 */
ssize_t tle_tcp_stream_writev(struct tle_stream *ts, struct rte_mempool *mp,
	const struct iovec *iov, int iovcnt);

/**
 * Back End (BE) API.
 * BE API functions are not multi-thread safe.
 * Supposed to be called by the L2/L3 processing layer.
 */

/**
 * Take input mbufs and distribute them to open TCP streams.
 * expects that for each input packet:
 *	- l2_len, l3_len, l4_len are setup correctly
 *	- (packet_type & (RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV6)) != 0,
 *	- (packet_type & RTE_PTYPE_L4_TCP) != 0,
 * During delivery L3/L4 checksums will be verified
 * (either relies on HW offload or in SW).
 * May cause some extra packets to be queued for TX.
 * This function is not multi-thread safe.
 * @param dev
 *   TCP device the packets were received from.
 * @param pkt
 *   The burst of input packets that need to be processed.
 * @param rp
 *   The array that will contain pointers of unprocessed packets at return.
 *   Should contain at least *num* elements.
 * @param rc
 *   The array that will contain error code for corresponding rp[] entry:
 *   - ENOENT - no open stream matching this packet.
 *   - ENOBUFS - receive buffer of the destination stream is full.
 *   Should contain at least *num* elements.
 * @param num
 *   Number of elements in the *pkt* input array.
 * @return
 *   number of packets delivered to the TCP streams.
 */
uint16_t tle_tcp_rx_bulk(struct tle_dev *dev, struct rte_mbuf *pkt[],
	struct rte_mbuf *rp[], int32_t rc[], uint16_t num);


/**
 * Take input mbufs and put them for processing to given TCP streams.
 * expects that for each input packet:
 *	- l2_len, l3_len, l4_len are setup correctly
 *	- (packet_type & (RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV6)) != 0,
 *	- (packet_type & RTE_PTYPE_L4_TCP) != 0,
 * During delivery L3/L4 checksums will be verified
 * (either relies on HW offload or in SW).
 * May cause some extra packets to be queued for TX.
 * This function is not multi-thread safe.
 * @param ts
 *   TCP stream given packets belong to.
 *   Note that it is caller repsonsibility to make sure that input packets
 *   belong to given stream.
 * @param pkt
 *   The burst of input packets that need to be processed.
 * @param rp
 *   The array that will contain pointers of unprocessed packets at return.
 *   Should contain at least *num* elements.
 * @param rc
 *   The array that will contain error code for corresponding rp[] entry:
 *   - ENOENT - invalid stream.
 *   - ENOBUFS - receive buffer of the destination stream is full.
 *   - EINVAL - invalid input packet encountered.
 *   Should contain at least *num* elements.
 * @param num
 *   Number of elements in the *pkt* input array.
 * @return
 *   number of packets delivered to the TCP stream.
 */
uint16_t tle_tcp_stream_rx_bulk(struct tle_stream *ts, struct rte_mbuf *pkt[],
	struct rte_mbuf *rp[], int32_t rc[], uint16_t num);

/**
 * Fill *pkt* with pointers to the packets that have to be transmitted
 * over given TCP device.
 * Output packets have to be ready to be passed straight to rte_eth_tx_burst()
 * without any extra processing.
 * TCP/IPv4 checksum either already calculated or appropriate mbuf fields set
 * properly for HW offload.
 * This function is not multi-thread safe.
 * @param dev
 *   TCP device the output packets will be transmitted over.
 * @param pkt
 *   An array of pointers to *rte_mbuf* structures that
 *   must be large enough to store up to *num* pointers in it.
 * @param num
 *   Number of elements in the *pkt* array.
 * @return
 *   number of of entries filled inside *pkt* array.
 */
uint16_t tle_tcp_tx_bulk(struct tle_dev *dev, struct rte_mbuf *pkt[],
	uint16_t num);

/**
 * perform internal processing for given TCP context.
 * Checks which timers are expired and performs the required actions
 * (retransmission/connection abort, etc.)
 * May cause some extra packets to be queued for TX.
 * This function is not multi-thread safe.
 * @param ctx
 *   TCP context to process.
 * @param num
 *   maximum number of streams to process.
 * @return
 *   zero on successful completion.
 *   - EINVAL - invalid parameter passed to function
 * @return
 */
int tle_tcp_process(struct tle_ctx *ctx, uint32_t num);

#ifdef __cplusplus
}
#endif

#endif /* _TLE_TCP_H_ */
