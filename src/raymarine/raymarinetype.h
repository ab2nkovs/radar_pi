
#ifdef INITIALIZE_RADAR

PLUGIN_BEGIN_NAMESPACE

static const NetworkAddress seatalk_hs_announce(224, 0, 0, 1, 5800);
static const NetworkAddress null_addr(0, 0, 0, 0, 0);

PLUGIN_END_NAMESPACE

#endif

#define RANGE_METRIC_RT_RAYMARINE \
  { 125, 250, 500, 750, 1500, 3000, 6000, 12000, 24000, 48000, 72000 }
// Garmin mixed range is the same as nautical miles, it does not support really short ranges
#define RANGE_MIXED_RT_RAYMARINE                                                                                          \
  {                                                                                                                       \
    1852 / 4, 1852 / 2, 1852, 1852 * 3 / 2, 1852 * 3, 1852 * 6, 1852 * 12, 1852 * 24, 1852 * 48, 1852 * 96, 1852 * 144                                                             \
  }
#define RANGE_NAUTIC_RT_RAYMARINE                                                                                         \
  {                                                                                                                       \
    1852 / 4, 1852 / 2, 1852, 1852 * 3 / 2, 1852 * 3, 1852 * 6, 1852 * 12, 1852 * 24, 1852 * 48, 1852 * 96, 1852 * 144                                         \
  }

// Garmin HD has 720 spokes of varying 519 - 705 bytes each
#define RAYMARINE_SPOKES 2048
#define RAYMARINE_MAX_SPOKE_LEN 1024

#if SPOKES_MAX < RAYMARINE_SPOKES
#undef SPOKES_MAX
#define SPOKES_MAX RAYMARINE_SPOKES
#endif
#if SPOKE_LEN_MAX < RAYMARINE_MAX_SPOKE_LEN
#undef SPOKE_LEN_MAX
#define SPOKE_LEN_MAX RAYMARINE_MAX_SPOKE_LEN
#endif

DEFINE_RADAR(RT_RAYMARINE,                              /* Type */
             wxT("Raymarine"),                          /* Name */
             RAYMARINE_SPOKES,                          /* Spokes */
             RAYMARINE_MAX_SPOKE_LEN,                   /* Spoke length */
             RaymarineControlsDialog,                    /* Controls class */
             RaymarineReceive(pi, ri, seatalk_hs_announce), /* Receive class */
             RaymarineControl(null_addr)                    /* Send/Control class */
)
