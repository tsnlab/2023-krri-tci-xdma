#ifndef ALINX_PTP_H
#define ALINX_PTP_H

#include "libxdma.h"
#include "tsn.h"

struct ptp_device_data *ptp_device_init(struct device *dev, struct xdma_dev *xdev);
void ptp_device_destroy(struct ptp_device_data *ptp);

#endif /* ALINX_PTP_H */
