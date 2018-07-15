/*
 * Filter configuration
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stm32f10x_can.h>
#include "can_driver.h"
#include "internal.h"

#define FILTER_FLAG_EFF  (1 << 2)
#define FILTER_FLAG_RTR  (1 << 1)

/*
 * Look at page 640 of Reference Manual, consider the Mapping of the filter registers.
 * Documentation is definitely unclear about ID/Mask bits mapping, and it took some time to understand it.
 * https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Flat.aspx?RootFolder=\
 * %2Fpublic%2FSTe2ecommunities%2Fmcu%2FLists%2Fcortex_mx_stm32%2FCAN%20filtering
 */
static void _buildIdMask(const CanasCanFilterConfig* cfg, uint32_t* pid, uint32_t* pmask)
{
    /*
     * If Mask asks us to accept both STDID and EXTID, we need to use EXT mode on filter, otherwise it will reject
     * all EXTID frames.
     * This logic is not documented at all, and it's really bad.
     */
    if ((cfg->id & CANAS_CAN_FLAG_EFF) || !(cfg->mask & CANAS_CAN_FLAG_EFF))
    {
        *pid   = (cfg->id   & CANAS_CAN_MASK_EXTID) << 3;
        *pmask = (cfg->mask & CANAS_CAN_MASK_EXTID) << 3;
        *pid |= FILTER_FLAG_EFF;
    }
    else
    {
        *pid   = (cfg->id   & CANAS_CAN_MASK_STDID) << 21;
        *pmask = (cfg->mask & CANAS_CAN_MASK_STDID) << 21;
    }

    if (cfg->id & CANAS_CAN_FLAG_RTR)
        *pid |= FILTER_FLAG_RTR;

    if (cfg->mask & CANAS_CAN_FLAG_EFF)
        *pmask |= FILTER_FLAG_EFF;

    if (cfg->mask & CANAS_CAN_FLAG_RTR)
        *pmask |= FILTER_FLAG_RTR;
}

static void _filt(int iface, int filt_index, const CanasCanFilterConfig* cfg)
{
    if (filt_index >= CAN_FILTERS_PER_IFACE)
        return;

    uint32_t id = 0, mask = 0;
    _buildIdMask(cfg, &id, &mask);

    if (iface != 0)
        filt_index += CAN_FILTERS_PER_IFACE;              // Add offset for CAN2

    const uint32_t filter_bit_pos = 1ul << filt_index;

    __disable_irq();
    CAN1->FMR |= 1;                                       // Enter initialization mode
    CAN1->FA1R &= ~filter_bit_pos;                        // Deactivate this filter

    if (cfg != NULL)                                      // cfg == NULL means that filter should be disabled
    {
        CAN1->FS1R |= filter_bit_pos;                     // Scale is 32-bit
        CAN1->FM1R &= ~filter_bit_pos;                    // Mode is ID/Mask
        CAN1->sFilterRegister[filt_index].FR1 = id;
        CAN1->sFilterRegister[filt_index].FR2 = mask;
        if (filt_index & 1)                               // FIFO load balancing: 0 or 1
            CAN1->FFA1R |= filter_bit_pos;                // FIFO 1
        else
            CAN1->FFA1R &= ~filter_bit_pos;               // FIFO 0

        CAN1->FA1R |= filter_bit_pos;                     // Activate filter
    }
    CAN1->FMR &= (uint32_t)~1;                            // Leave initialization mode
    __enable_irq();
}

static void _filtAcceptEverything(int iface)
{
    CanasCanFilterConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    for (int i = 0; i < CAN_FILTERS_PER_IFACE; i++)
        _filt(iface, i, &cfg);
}

int canFilterInit(void)
{
    CAN_SlaveStartBank(CAN_FILTERS_PER_IFACE);
    _filtAcceptEverything(0);
    _filtAcceptEverything(1);
    return 0;
}

int canFilterSetup(int iface, const CanasCanFilterConfig* pfilters, int filters_len)
{
    if ((iface < 0 || iface >= CAN_IFACE_COUNT) || pfilters == NULL || filters_len <= 0)
        return -1;

    _filtAcceptEverything(iface);              // Allow all messages until the filters are configured.

    if (filters_len > CAN_FILTERS_PER_IFACE)   // We need to allow all messages because there is not enough filters.
        return 0;                              // It's kinda okay.

    for (int i = 0; i < CAN_FILTERS_PER_IFACE; i++)
    {
        if (filters_len-- > 0)
            _filt(iface, i, &pfilters[i]);
        else
            _filt(iface, i, NULL);             // Disable last filters because they are not used
    }
    return 0;
}
