/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "gpio_pins.h"

gpio_config_t gpioRL1 = {
    "out_RL1",                                       	/* name */
    &IOMUXC_SW_MUX_CTL_PAD_ENET1_RGMII_TD2,             /* muxReg */
    5,                                              	/* muxConfig */
    &IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD2,             /* padReg */
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD2_PS(2) |       /* padConfig */
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD2_PE_MASK |
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD2_HYS_MASK,
    GPIO7,                                          	/* base */
    8                                               	/* pin */
};

gpio_config_t gpioRL2 = {
    "out_RL2",                                       	/* name */
    &IOMUXC_SW_MUX_CTL_PAD_ECSPI2_SS0,          	/* muxReg */
    5,                                               	/* muxConfig */
    &IOMUXC_SW_PAD_CTL_PAD_ECSPI2_SS0,          	/* padReg */
    IOMUXC_SW_PAD_CTL_PAD_ECSPI2_SS0_PS(2) |    	/* padConfig */
    IOMUXC_SW_PAD_CTL_PAD_ECSPI2_SS0_PE_MASK |
    IOMUXC_SW_PAD_CTL_PAD_ECSPI2_SS0_HYS_MASK,
    GPIO4,                                           	/* base */
    23                                               	/* pin */
};

gpio_config_t gpioINT = {
    "in_INT",                                       	/* name */
    &IOMUXC_SW_MUX_CTL_PAD_ENET1_RGMII_TD1,             /* muxReg */
    5,                                              	/* muxConfig */
    &IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD1,             /* padReg */
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD1_PS(2) |       /* padConfig */
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD1_PE_MASK |
    IOMUXC_SW_PAD_CTL_PAD_ENET1_RGMII_TD1_HYS_MASK,
    GPIO7,                                          	/* base */
    7                                               	/* pin */
};

void configure_gpio_pin(gpio_config_t *config)
{
    assert(config);

    *(config->muxReg) = config->muxConfig;
    *(config->padReg) = config->padConfig;
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
