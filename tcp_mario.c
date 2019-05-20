/*
 * TCP Mario congestion control
 *
 * @author Fan Jiang 
 *
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>
#include <linux/sysctl.h>

#include <net/tcp.h>

#define SAMPLE_SIZE 100
#define INIT_FACTOR 10

static u32 bandwidth;
static u32 base_cwnd;
static u32 rtt, rtt_cnt;
static u32 factor = INIT_FACTOR;

/**
 * init() 
 *
 * Initialize the congestion window size with bytes 
 * sent per second over the specific bandwidth, in 
 * order to skip slow-start phase 
 *
 */
static void tcp_mario_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

        base_cwnd = bandwidth << 7;
        rtt = 0;
        rtt_cnt = 0;
        tp->snd_cwnd = base_cwnd;
}

/**
 * cong_avoid()
 *
 * Recover congestion window size in case that the 
 * window size is reduced by kernel 
 *
 */
static void tcp_mario_cong_avoid(struct sock *sk, u32 ack, 
                                u32 acked, u32 in_flight)
{
        struct tcp_sock *tp = tcp_sk(sk);

        tp->snd_cwnd = base_cwnd;
}

/**
 * ssthresh()
 *
 * Recover congestion window size in case that the
 * window size is reduced by kernel. Since slow-start 
 * phase is nonexistent, the function returns the 
 * calculated congestion window size
 *
 */
static u32 tcp_mario_ssthresh(struct sock *sk)
{
        struct tcp_sock *tp = tcp_sk(sk);

        tp->snd_cwnd = base_cwnd;

        return base_cwnd;
}

/**
 * pkts_acked()
 *
 * Sample RTTs from ACKs to calculate the optimal congestion 
 * window size for the given bandwidth
 *
 */
static void tcp_mario_pkts_acked(struct sock *sk, u32 num_acked, 
                                s32 rtt_us)
{
    u32 avg_rtt;

    if(rtt_cnt > SAMPLE_SIZE){
        return;
    }
    rtt_us /= 1000;
    
    if(rtt_us > 0 && rtt_us < 300){
        rtt_cnt++;
        rtt += rtt_us;
    }
    if(rtt_cnt == SAMPLE_SIZE){
        avg_rtt = rtt/rtt_cnt;
        base_cwnd = 1 + bandwidth << 10 * avg_rtt/(factor * 1000); 
    }

}

/**
 * undo_cwnd()
 *
 * Recover congestion window size when packet loss occurs, 
 * in case that the window size is reduced by kernel
 *
 */
static u32 tcp_mario_undo_cwnd(struct sock *sk)
{
        struct tcp_sock *tp = tcp_sk(sk);

        tp->snd_cwnd = base_cwnd;

        return tp->snd_cwnd;

}

static struct tcp_congestion_ops tcp_mario __read_mostly = {
	.flags		= TCP_CONG_RTT_STAMP,
	.init		= tcp_mario_init,
	.ssthresh	= tcp_mario_ssthresh,
        .pkts_acked     = tcp_mario_pkts_acked,
	.cong_avoid	= tcp_mario_cong_avoid,
        .undo_cwnd      = tcp_mario_undo_cwnd,

	.owner		= THIS_MODULE,
	.name		= "mario",
};

/**
 * Global variables can be modified by user via /proc/sys 
 *
 */
static ctl_table tcp_mario_table[] = {
    {
        .procname       = "bandwidth",
        .data           = &bandwidth,
        .maxlen         = sizeof(u32),
        .mode           = 0666,
        .proc_handler   = &proc_dointvec,
    },
    {
        .procname       = "factor",
        .data           = &factor,
        .maxlen         = sizeof(u32),
        .mode           = 0666,
        .proc_handler   = &proc_dointvec
    },
    {}
};

static struct ctl_table_header *hdr;

static int __init tcp_mario_register(void)
{
	tcp_register_congestion_control(&tcp_mario);
        
        //set up /proc/sys entries
        hdr = register_net_sysctl(&init_net, "net/ipv4/tcp_mario", tcp_mario_table);
	
        return 0;
}

static void __exit tcp_mario_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_mario);
        
        //clear /proc/sys entries on exit
        if(hdr != NULL){
            unregister_net_sysctl_table(hdr);
        }
}

module_init(tcp_mario_register);
module_exit(tcp_mario_unregister);

MODULE_AUTHOR("Fan Jiang");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Mario");
