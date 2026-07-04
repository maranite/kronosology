#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
gen_deax_vectors.py - an independent, from-scratch Python re-implementation
of the DEAX stream cipher, written directly from KronosExtract/source/
kronos_extract.c's own deax_step()/deax_init() (not from the C++ port in
chip_state.cpp), to catch any transcription bug the port itself wouldn't
reveal -- same technique used for cb32/Blowfish elsewhere in this project.

Run: python3 gen_deax_vectors.py
Prints deax_step() outputs for a short, arbitrary but fixed input sequence,
used as the KAT vector hardcoded into test_chip_state.cpp's test [6].
"""


class Deax:
    def __init__(self):
        self.gpa = 0
        self.RA = self.RB = self.RC = self.RD = self.RE = self.RF = self.RG = 0
        self.SA = self.SB = self.SC = self.SD = self.SE = self.SF = self.SG = 0
        self.TA = self.TB = self.TC = self.TD = self.TE = 0

    def step(self, in_byte):
        v = (in_byte ^ self.gpa) & 0xFF
        bl_in = ((v >> 5) | ((v & 0xF) << 3)) & 0xFF
        dl_in = v & 0x1F

        # R-bank: new_RA = rot5L1(RG) + RD, mod-5bit
        rg_rot = (((self.RG & 0xF) << 1) | (self.RG >> 4)) & 0xFF
        sum_r = (rg_rot + self.RD) & 0xFF
        new_ra = (sum_r - 0x1F) & 0xFF if sum_r >= 0x20 else sum_r
        old_rd = self.RD
        old_ra, old_rb, old_rc = self.RA, self.RB, self.RC
        old_re, old_rf = self.RE, self.RF
        self.RA = new_ra
        self.RB = old_ra
        self.RC = old_rb
        self.RD = (dl_in ^ old_rc) & 0xFF
        self.RE = old_rd
        self.RF = old_re
        self.RG = old_rf
        cl_t = (new_ra ^ old_rd) & 0xFF

        # S-bank: new_SA = rot7L1(SG) + SF, mod-7bit
        sg_rot = (((self.SG & 0x3F) << 1) | (self.SG >> 6)) & 0xFF
        sum_s = (sg_rot + self.SF) & 0xFF
        new_sa = (sum_s - 0x7F) & 0xFF if sum_s >= 0x80 else sum_s
        old_sa = self.SA
        old_sb, old_sc, old_sd, old_se, old_sf = (
            self.SB, self.SC, self.SD, self.SE, self.SF)
        self.SA = new_sa
        self.SB = old_sa
        self.SC = old_sb
        self.SD = old_sc
        self.SE = old_sd
        self.SF = (bl_in ^ old_se) & 0xFF
        self.SG = old_sf

        # T-bank: new_TA = TC + TE, mod-5bit
        sum_t = (self.TC + self.TE) & 0xFF
        new_ta = (sum_t - 0x1F) & 0xFF if sum_t >= 0x20 else sum_t
        old_tc = self.TC
        old_ta, old_tb, old_td = self.TA, self.TB, self.TD
        self.TA = new_ta
        self.TB = old_ta
        self.TC = ((v >> 3) ^ old_tb) & 0xFF
        self.TD = old_tc
        self.TE = old_td

        # MUX and GPA update
        bl_t = (new_ta ^ old_tc) & 0xFF
        mux = (bl_t & new_sa) | ((~new_sa) & 0xFF & cl_t)
        self.gpa = (((self.gpa & 0xF) << 4) | (mux & 0xF)) & 0xFF


if __name__ == "__main__":
    d = Deax()
    inputs = [0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07]
    outputs = []
    for b in inputs:
        d.step(b)
        outputs.append(d.gpa)
    print("inputs: ", [hex(x) for x in inputs])
    print("gpa after each step:", [hex(x) for x in outputs])
