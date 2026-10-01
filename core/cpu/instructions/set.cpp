#include "set.h"

#include <print>

#include "../cpu.h"

InstructionSet::InstructionSet() :
    root(
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.aH()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.aL()); }
    )
{
    root.Add(0x0, 0x0, {
        "NOP",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            // nothing!!
        }

    });

    root.Add(0x0, 0x7, {
        "LDC.B #xx:8, CCR",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            cpu.reg.flags.CCR = imm;
        }
    });

    root.Add(0x0, 0x4, {
        "ORC #xx:8, CCR",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu) { cpu.reg.flags.CCR |= cpu.b(); }
    });
    root.Add(0x0, 0x5, {
        "XORC #xx:8, CCR",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu) { cpu.reg.flags.CCR ^= cpu.b(); }
    });
    root.Add(0x0, 0x6, {
        "ANDC #xx:8, CCR",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu) { cpu.reg.flags.CCR &= cpu.b(); }
    });

    root.Add(0x0, 0x8, {
        "ADD.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd = cpu.reg.Add(*rd, *rs);
        }
    });

    root.Add(0x0, 0x9, {
        "ADD.W Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd = cpu.reg.Add(*rd, *rs);
        }
    });

    root.AddSubtable(0x0, 0xA,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x0A, 0x00, {
                "INC.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());
                    *rd = cpu.reg.Inc(*rd, 1);
                }
            });

            table.Add(0x0A, {0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
                "ADD.L ERs, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd = cpu.reg.Add(*erd, *ers);
                }
            });
        });

    root.Add(0x0, 0xC, {
        "MOV.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd = *rs;
            cpu.reg.MovFlags(*rd);
        }
    });


    root.AddSubtable(0x0, 0xB,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x0B, 0x00, {
                "ADDS #1, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd += 1;
                }
            });

            table.Add(0x0B, 0x5, {
                "INC.W #1, Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.reg.Inc(*rd, 1);
                }
            });

            table.Add(0x0B, 0xD, {
                "INC.W #2, Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.reg.Inc(*rd, 2);
                }
            });

            table.Add(0x0B, 0x07, {
                "INC.L #1, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());

                    *erd = cpu.reg.Inc(*erd, 1);
                }
            });

            table.Add(0x0B, 0x08, {
                "ADDS #2, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd += 2;
                }
            });

            table.Add(0x0B, 0x09, {
                "ADDS #4, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd += 4;
                }
            });
        });


    root.Add(0x0, 0xD, {
        "MOV.W Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd = *rs;
            cpu.reg.MovFlags(*rd);
        }
    });


    root.AddSubtable(0x0, 0xF,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x0F, {0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
                "MOV.L ERs, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    *erd = *ers;

                    cpu.reg.MovFlags(*erd);
                }
            });
        });

    root.AddSubtable(0x0, 0x1,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.AddSubtable(0x01, 0x0,
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cH()); },
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
                [](InstructionTable& table)
                {
                    table.AddSubtable(0x06, 0x09,
                        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.c()); },
                        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.dH() >> 3) & 1); },
                        [](InstructionTable& table)
                        {
                            table.Add(0x69, 0x0, {
                                "MOV.L @ERs, ERd",
                                4,
                                {2, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dH());
                                    uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                                    *erd = cpu.mem->Read32(*ers);
                                    cpu.reg.MovFlags(*erd);
                                }
                            });

                            table.Add(0x69, 0x1, {
                                "MOV.L ERs, @ERd",
                                4,
                                {2, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const uint32_t* erd = cpu.reg.Reg32(cpu.dH());
                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dL());

                                    cpu.mem->Write32(*erd, *ers);
                                    cpu.reg.MovFlags(*ers);
                                }
                            });
                        }
                    );

                    table.AddSubtable(0x06, 0x0B,
                        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.c()); },
                        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.dH()); },
                        [](InstructionTable& table)
                        {
                            table.Add(0x6B, 0x0, {
                                "MOV.L @aa:16, ERd",
                                6,
                                {3, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const uint16_t abs = cpu.ef();
                                    uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                                    *erd = cpu.mem->Read32(abs);
                                    cpu.reg.MovFlags(*erd);
                                }
                            });

                            table.Add(0x6B, 0x8, {
                                "MOV.L ERs, @aa:16",
                                6,
                                {3, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const uint16_t abs = cpu.ef();
                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dL());

                                    cpu.mem->Write32(abs, *ers);
                                    cpu.reg.MovFlags(*ers);
                                }
                            });
                        }
                    );

                    table.AddSubtable(0x06, 0x0D,
                        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.c()); },
                        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.dH() >> 3) & 1); },
                        [](InstructionTable& table)
                        {
                            table.Add(0x6D, 0x1, {
                                "MOV.L ERs, @-ERd",
                                4,
                                {2, 0, 0, 0, 2, 2},
                                [](CPU& cpu)
                                {
                                    uint32_t* erd = cpu.reg.Reg32(cpu.dH());
                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dL());

                                    *erd -= 4;

                                    cpu.mem->Write32(*erd, *ers);

                                    cpu.reg.MovFlags(*ers);
                                }
                            });

                            table.Add(0x6D, 0x0, {
                                "MOV.L @ERs+, ERd",
                                4,
                                {2, 0, 0, 0, 2, 2},
                                [](CPU& cpu)
                                {
                                    uint32_t* ers = cpu.reg.Reg32(cpu.dH());
                                    uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                                    *erd = cpu.mem->Read32(*ers);
                                    *ers += 4;

                                    cpu.reg.MovFlags(*erd);
                                }
                            });
                        }
                    );

                    table.AddSubtable(0x06, 0x0F,
                        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.c()); },
                        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.dH() >> 3) & 1); },
                        [](InstructionTable& table)
                        {
                            table.Add(0x6F, 0x0, {
                                "MOV.L @(d:16,ERs), ERd",
                                6,
                                {3, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const auto disp = static_cast<int16_t>(cpu.ef());

                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dH());
                                    uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                                    *erd = cpu.mem->Read32(*ers + disp);

                                    cpu.reg.MovFlags(*erd);
                                }
                            });

                            table.Add(0x6F, 0x1, {
                                "MOV.L ERs, @(d:16,ERd)",
                                6,
                                {3, 0, 0, 0, 2, 0},
                                [](CPU& cpu)
                                {
                                    const auto disp = static_cast<int16_t>(cpu.ef());

                                    const uint32_t* erd = cpu.reg.Reg32(cpu.dH());
                                    const uint32_t* ers = cpu.reg.Reg32(cpu.dL());

                                    cpu.mem->Write32(*erd + disp, *ers);

                                    cpu.reg.MovFlags(*ers);
                                }
                            });
                        }
                    );
                }
            );

            table.Add(0x01, 0x8, {
                "SLEEP",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    cpu.sleep = true;
                }
            });

            table.AddSubtable(0x01, 0xC,
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
                [](InstructionTable& table)
                {
                    table.Add(0x01C05, 0x2, {
                        "MULXS.W Rs, ERd",
                        4,
                        {2, 0, 0, 0, 0, 20},
                        [](CPU& cpu)
                        {
                            const uint16_t* rs = cpu.reg.Reg16(cpu.dH());
                            uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                            const auto destination_word = static_cast<int16_t>(static_cast<uint16_t>(*erd));
                            const int32_t result = static_cast<int32_t>(destination_word) *
                                                   static_cast<int16_t>(*rs);
                            *erd = result;

                            cpu.reg.flags.Z = result == 0;
                            cpu.reg.flags.N = result < 0;
                        }
                    });
                });

            table.AddSubtable(0x01, 0xD,
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
                [](InstructionTable& table)
                {
                    table.Add(0x01D05, 0x3, {
                        "DIVXS.W Rs, ERd",
                        4,
                        {2, 0, 0, 0, 0, 20},
                        [](CPU& cpu)
                        {
                            const uint16_t* rs = cpu.reg.Reg16(cpu.dH());
                            uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                            const auto quotient = static_cast<int16_t>(static_cast<int32_t>(*erd) / static_cast<int16_t>
                                (*rs));
                            const auto remainder = static_cast<int16_t>(static_cast<int32_t>(*erd) % static_cast<
                                int16_t>(*rs));

                            *erd = (remainder << 16) | quotient;

                            cpu.reg.flags.Z = quotient == 0;
                            cpu.reg.flags.N = quotient < 0;
                        }
                    });
                });


            table.AddSubtable(0x01, 0xF,
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
                [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
                [](InstructionTable& table)
                {
                    table.Add(0x01F06, 0x5, {
                        "XOR.L ERs, ERd",
                        4,
                        {3, 0, 0, 0, 0, 0},
                        [](CPU& cpu)
                        {
                            const uint32_t* ers = cpu.reg.Reg32(cpu.dH());
                            uint32_t* erd = cpu.reg.Reg32(cpu.dL());

                            *erd ^= *ers;
                            cpu.reg.MovFlags(*erd);
                        }
                    });
                });
        }
    );

    root.AddSubtable(0x1, 0x0,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x10, 0x00, {
                "SHLL.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());
                    cpu.reg.flags.C = *rd & NEGATIVE_MASK(8);
                    *rd <<= 1;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x10, 0x01, {
                "SHLL.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    cpu.reg.flags.C = *rd & NEGATIVE_MASK(16);
                    *rd <<= 1;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x10, 0x03, {
                "SHLL.L ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    cpu.reg.flags.C = *erd & NEGATIVE_MASK(32);
                    *erd <<= 1;
                    cpu.reg.MovFlags(*erd);
                }
            });
        });

    root.AddSubtable(0x1, 0x1,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x11, 0x00, {
                "SHLR.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());
                    cpu.reg.flags.C = *rd & 1;
                    *rd >>= 1;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x11, 0x01, {
                "SHLR.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    cpu.reg.flags.C = *rd & 1;
                    *rd >>= 1;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x11, 0x03, {
                "SHLR.L ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    cpu.reg.flags.C = *erd & 1;
                    *erd >>= 1;
                    cpu.reg.MovFlags(*erd);
                }
            });

            table.Add(0x11, 0x09, {
                "SHAR.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    cpu.reg.flags.C = *rd & 1;
                    *rd = (*rd >> 1) | (*rd & NEGATIVE_MASK(16));
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x11, 0x0B, {
                "SHAR.L ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    cpu.reg.flags.C = *erd & 1;
                    *erd = (*erd >> 1) | (*erd & NEGATIVE_MASK(32));
                    cpu.reg.MovFlags(*erd);
                }
            });
        });


    root.AddSubtable(0x1, 0x2,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x12, 0x08, {
                "ROTL.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    const uint8_t msb = (*rd >> 7) & 1;
                    *rd = (*rd << 1) | msb;
                    cpu.reg.flags.C = msb;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x12, 0x09, {
                "ROTL.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    const uint8_t msb = (*rd >> 15) & 1;
                    *rd = (*rd << 1) | msb;
                    cpu.reg.flags.C = msb;
                    cpu.reg.MovFlags(*rd);
                }
            });
        });


    root.Add(0x1, 0x4, {
        "OR.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd |= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0x1, 0x5, {
        "XOR.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd ^= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0x1, 0x6, {
        "AND.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd &= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.AddSubtable(0x1, 0x7,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x17, 0x00, {
                "NOT.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());
                    *rd = ~(*rd);
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x17, 0x05, {
                "EXTU.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    *rd &= 0xFF;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x17, 0x07, {
                "EXTU.L ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd &= 0xFFFF;
                    cpu.reg.MovFlags(*erd);
                }
            });

            table.Add(0x17, 0x08, {
                "NEG.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.reg.Sub(static_cast<uint8_t>(0), *rd);
                }
            });

            table.Add(0x17, 0x09, {
                "NEG.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.reg.Sub(static_cast<uint16_t>(0), *rd);
                }
            });

            table.Add(0x17, 0x0D, {
                "EXTS.W Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    *rd = static_cast<int16_t>(static_cast<int8_t>(*rd & 0xFF));
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x17, 0x0F, {
                "EXTS.L ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* rd = cpu.reg.Reg32(cpu.bL());
                    *rd = static_cast<int32_t>(static_cast<int16_t>(*rd & 0xFFFF));
                    cpu.reg.MovFlags(*rd);
                }
            });
        });

    root.Add(0x1, 0x8, {
        "SUB.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd = cpu.reg.Sub(*rd, *rs);
        }
    });

    root.Add(0x1, 0x9, {
        "SUB.W Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd = cpu.reg.Sub(*rd, *rs);
        }
    });


    root.AddSubtable(0x1, 0xA,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x1A, 0x00, {
                "DEC.B Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.reg.Dec(*rd, 1);
                }
            });

            table.Add(0x1A, {0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
                "SUB.L ERs, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());;

                    *erd = cpu.reg.Sub(*erd, *ers);
                }
            });
        });

    root.AddSubtable(0x1, 0xB, [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x1B, 0x05, {
                "DEC.W #1, Rd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());
                    *rd = cpu.reg.Dec(*rd, 1);
                }
            });

            table.Add(0x1B, 0x08, {
                "SUBS #2, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd -= 2;
                }
            });

            table.Add(0x1B, 0x09, {
                "SUBS #4, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());
                    *erd -= 4;
                }
            });
        });

    root.Add(0x1, 0xC, {
        "CMP.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            const uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            cpu.reg.Sub(*rd, *rs);
        }
    });


    root.Add(0x1, 0xD, {
        "CMP.W Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            const uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            cpu.reg.Sub(*rd, *rs);
        }
    });

    root.Add(0x1, 0xE, {
        "SUBX Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd = cpu.reg.Sub(*rd, static_cast<uint8_t>(*rs + cpu.reg.flags.C));
        }
    });

    root.AddSubtable(0x1, 0xF,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x1F, {0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
                "CMP.L ERs, ERd",
                2,
                {1, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());;

                    cpu.reg.Sub(*erd, *ers);
                }
            });
        });

    root.Add(0x2, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "MOV.B @aa:8, Rd",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());
            const uint8_t abs = cpu.b();

            const uint16_t address = 0xFF00 | abs;

            *rd = cpu.mem->Read8(address);
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0x3, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "MOV.B Rs, @aa:8",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.aL());
            const uint8_t abs = cpu.b();

            const uint16_t address = 0xFF00 | abs;

            cpu.mem->Write8(address, *rs);
            cpu.reg.MovFlags(*rs);
        }
    });

    root.Add(0x4, 0x0, {
        "BRA d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x2, {
        "BHI d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (!(cpu.reg.flags.C || cpu.reg.flags.Z))
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x3, {
        "BLS d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.C || cpu.reg.flags.Z)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x4, {
        "BCC d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (!cpu.reg.flags.C)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x5, {
        "BCS d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.C)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x6, {
        "BNE d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (!cpu.reg.flags.Z)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0x7, {
        "BEQ d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.Z)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xA, {
        "BPL d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (!cpu.reg.flags.N)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xB, {
        "BMI d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.N)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xC, {
        "BGE d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.N == cpu.reg.flags.V)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xD, {
        "BLT d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.N != cpu.reg.flags.V)
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xE, {
        "BGT d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (!(cpu.reg.flags.Z || (cpu.reg.flags.N != cpu.reg.flags.V)))
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x4, 0xF, {
        "BLE d:8",
        2,
        {2, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const auto disp = static_cast<int8_t>(cpu.b());
            if (cpu.reg.flags.Z || (cpu.reg.flags.N != cpu.reg.flags.V))
                cpu.reg.PC += disp;
        }
    });

    root.Add(0x5, 0x0, {
        "MULXU.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 20},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd = (*rd & 0xFF) * (*rs);
        }
    });


    root.Add(0x5, 0x1, {
        "DIVXU.B Rs, Rd",
        2,
        {1, 0, 0, 0, 0, 12},
        [](CPU& cpu)
        {
            const uint8_t* rs = cpu.reg.Reg8(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            const uint16_t quotient = *rd / *rs;
            const uint16_t remainder = *rd % *rs;

            *rd = (remainder << 8) | quotient;

            cpu.reg.flags.Z = quotient == 0;
            cpu.reg.flags.N = quotient & NEGATIVE_MASK(8);
        }
    });


    root.Add(0x5, 0x2, {
        "MULXU.W Rs, ERd",
        2,
        {1, 0, 0, 0, 0, 20},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint32_t* erd = cpu.reg.Reg32(cpu.bL());

            *erd = (*erd & 0xFFFF) * (*rs);
        }
    });

    root.Add(0x5, 0x3, {
        "DIVXU.W Rs, ERd",
        2,
        {1, 0, 0, 0, 0, 20},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint32_t* erd = cpu.reg.Reg32(cpu.bL());

            const uint16_t quotient = *erd / *rs;
            const uint16_t remainder = *erd % *rs;

            *erd = (remainder << 16) | quotient;

            cpu.reg.flags.Z = quotient == 0;
            cpu.reg.flags.N = quotient & NEGATIVE_MASK(16);
        }
    });

    root.Add(0x5, 0x4, {
        "RTS",
        2,
        {2, 0, 1, 0, 0, 2},
        [](CPU& cpu)
        {
            cpu.reg.PC = cpu.Pop16();
        },
        true
    });

    root.Add(0x5, 0x6, {
        "RTE",
        2,
        {2, 0, 2, 0, 0, 2},
        [](CPU& cpu)
        {
            cpu.reg.PC = cpu.Pop16();
            cpu.reg.flags.CCR = cpu.Pop16() >> 8;
        },
        true
    });

    root.Add(0x5, 0x5, {
        "BSR d:8",
        2,
        {2, 0, 1, 0, 0, 0},
        [](CPU& cpu)
        {
            cpu.Push16(cpu.reg.PC + 2);

            const auto disp = static_cast<int8_t>(cpu.b());
            cpu.reg.PC += disp;
        }
    });

    root.AddSubtable(0x5, 0x8,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x58, 0x02, {
                "BHI d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (!(cpu.reg.flags.C || cpu.reg.flags.Z))
                        cpu.reg.PC += disp;
                }
            });

            table.Add(0x58, 0x03, {
                "BLS d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (cpu.reg.flags.C || cpu.reg.flags.Z)
                        cpu.reg.PC += disp;
                }
            });

            table.Add(0x58, 0x04, {
                "BCC d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (!cpu.reg.flags.C)
                        cpu.reg.PC += disp;
                }
            });

            table.Add(0x58, 0x05, {
                "BCS d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (cpu.reg.flags.C)
                        cpu.reg.PC += disp;
                }
            });


            table.Add(0x58, 0x06, {
                "BNE d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (!cpu.reg.flags.Z)
                        cpu.reg.PC += disp;
                }
            });


            table.Add(0x58, 0x07, {
                "BEQ d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (cpu.reg.flags.Z)
                        cpu.reg.PC += disp;
                }
            });

            table.Add(0x58, 0xC, {
                "BGE d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (cpu.reg.flags.N == cpu.reg.flags.V)
                        cpu.reg.PC += disp;
                }
            });

            table.Add(0x58, 0xD, {
                "BLT d:16",
                4,
                {2, 0, 0, 0, 0, 2},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());
                    if (cpu.reg.flags.N != cpu.reg.flags.V)
                        cpu.reg.PC += disp;
                }
            });
        });

    root.Add(0x5, 0x9, {
        "JMP @ERn",
        4,
        {2, 0, 0, 0, 0, 2},
        [](CPU& cpu)
        {
            const uint32_t* ern = cpu.reg.Reg32(cpu.bH());
            cpu.reg.PC = *ern & 0xFFFF; // TODO raise limit for advanced mode
        },
        true
    });

    root.Add(0x5, 0xA, {
        "JMP @aa:24",
        4,
        {2, 0, 0, 0, 0, 2},
        [](CPU& cpu)
        {
            const uint32_t abs = (cpu.b() << 16) | cpu.cd();
            cpu.reg.PC = abs;
        },
        true
    });

    root.Add(0x5, 0xD, {
        "JSR @ERn",
        2,
        {2, 0, 1, 0, 0, 0},
        [](CPU& cpu)
        {
            cpu.Push16(cpu.reg.PC + 2);

            const uint32_t* ern = cpu.reg.Reg32(cpu.bH());
            cpu.reg.PC = *ern & 0xFFFF; // TODO raise limit for advanced mode
        },
        true
    });

    root.Add(0x5, 0xE, {
        "JSR @aa:24",
        4,
        {2, 0, 1, 0, 0, 2},
        [](CPU& cpu)
        {
            cpu.Push16(cpu.reg.PC + 4);

            const uint32_t abs = (cpu.b() << 16) | cpu.cd();
            cpu.reg.PC = abs;
        },
        true
    });

    root.Add(0x6, 0x0, {
        "BSET Rn, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t* rn = cpu.reg.Reg8(cpu.bH());
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd |= 1 << *rn;
        }
    });

    root.Add(0x6, 0x4, {
        "OR.W Rs, Rd",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd |= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0x6, 0x5, {
        "XOR.W Rs, Rd",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd ^= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0x6, 0x6, {
        "AND.W Rs, Rd",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            const uint16_t* rs = cpu.reg.Reg16(cpu.bH());
            uint16_t* rd = cpu.reg.Reg16(cpu.bL());

            *rd &= *rs;
            cpu.reg.MovFlags(*rd);
        }
    });


    root.Add(0x6, 0x7, {
        "BST #xx:3, Rd",
        2,
        {1, 0, 0, 1, 0, 0},
        [](CPU& cpu)
        {
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());
            const uint8_t imm = cpu.bH();

            if (cpu.reg.flags.C)
            {
                *rd |= (1 << imm);
            }
            else
            {
                *rd &= ~(1 << imm);
            }
        }
    });

    root.AddSubtable(0x6, 0x8,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x68, 0x00, {
                "MOV.B @ERs, Rd",
                2,
                {1, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.mem->Read8(*ers);
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x68, 0x01, {
                "MOV.B Rs, @ERd",
                2,
                {1, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t* rs = cpu.reg.Reg8(cpu.bL());

                    cpu.mem->Write8(*erd, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        }
    );

    root.AddSubtable(0x6, 0x9,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x69, 0x00, {
                "MOV.W @ERs, Rd",
                2,
                {1, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.mem->Read16(*ers);
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x69, 0x01, {
                "MOV.W Rs, @ERd",
                2,
                {1, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint16_t* rs = cpu.reg.Reg16(cpu.bL());

                    cpu.mem->Write16(*erd, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        }
    );

    root.AddSubtable(0x6, 0xA,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x6A, 0x00, {
                "MOV.B @aa:16, Rd",
                4,
                {2, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t abs = cpu.cd();
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.mem->Read8(abs);
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x6A, 0x08, {
                "MOV.B Rs, @aa:16",
                4,
                {2, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t abs = cpu.cd();
                    const uint8_t* rs = cpu.reg.Reg8(cpu.bL());

                    cpu.mem->Write8(abs, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        }
    );


    root.AddSubtable(0x6, 0xB,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x6B, 0x0, {
                "MOV.W @aa:16, Rd",
                4,
                {2, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const uint16_t abs = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.mem->Read16(abs);
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x6B, 0x08, {
                "MOV.W Rs, @aa:16",
                4,
                {2, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const uint16_t abs = cpu.cd();
                    const uint16_t* rs = cpu.reg.Reg16(cpu.bL());

                    cpu.mem->Write16(abs, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        }
    );

    root.AddSubtable(0x6, 0xC,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x6C, 0x00, {
                "MOV.B @ERs+, Rd",
                2,
                {1, 0, 0, 1, 0, 2},
                [](CPU& cpu)
                {
                    uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.mem->Read8(*ers);
                    cpu.reg.MovFlags(*rd);

                    *ers += 1;
                }
            });

            table.Add(0x6C, 0x01, {
                "MOV.B Rs, @-ERd",
                2,
                {1, 0, 0, 1, 0, 2},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t* rs = cpu.reg.Reg8(cpu.bL());

                    *erd -= 1;

                    cpu.mem->Write8(*erd, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        });


    root.AddSubtable(0x6, 0xD,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x6D, 0x00, {
                "MOV.W @ERs+, Rd",
                2,
                {1, 0, 0, 0, 1, 2},
                [](CPU& cpu)
                {
                    uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.mem->Read16(*ers);
                    cpu.reg.MovFlags(*rd);

                    *ers += 2;
                }
            });

            table.Add(0x6D, 0x01, {
                "MOV.W Rs, @-ERd",
                2,
                {1, 0, 0, 0, 1, 2},
                [](CPU& cpu)
                {
                    uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint16_t* rs = cpu.reg.Reg16(cpu.bL());

                    *erd -= 2;

                    cpu.mem->Write16(*erd, *rs);
                    cpu.reg.MovFlags(*rs);
                }
            });
        });

    root.AddSubtable(0x6, 0xE,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x6E, 0x00, {
                "MOV.B @(d:16,ERs), Rd",
                4,
                {2, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());

                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint8_t* rd = cpu.reg.Reg8(cpu.bL());

                    *rd = cpu.mem->Read8(*ers + disp);

                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x6E, 0x01, {
                "MOV.B Rs, @(d:16,ERd)",
                4,
                {2, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());

                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t* rs = cpu.reg.Reg8(cpu.bL());

                    cpu.mem->Write8(*erd + disp, *rs);

                    cpu.reg.MovFlags(*rs);
                }
            });
        });

    root.AddSubtable(0x6, 0xF,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>((cpu.bH() >> 3) & 1); },
        [](InstructionTable& table)
        {
            table.Add(0x6F, 0x00, {
                "MOV.W @(d:16,ERs), Rd",
                4,
                {2, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());

                    const uint32_t* ers = cpu.reg.Reg32(cpu.bH());
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.mem->Read16(*ers + disp);

                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x6F, 0x01, {
                "MOV.W Rs, @(d:16,ERd)",
                4,
                {2, 0, 0, 0, 1, 0},
                [](CPU& cpu)
                {
                    const auto disp = static_cast<int16_t>(cpu.cd());

                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint16_t* rs = cpu.reg.Reg16(cpu.bL());

                    cpu.mem->Write16(*erd + disp, *rs);

                    cpu.reg.MovFlags(*rs);
                }
            });
        });

    root.Add(0x7, 0x0, {
        "BSET #xx:3, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.bH();
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd |= 1 << imm;
        }
    });

    root.Add(0x7, 0x2, {
        "BCLR #xx:3, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.bH();
            uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            *rd &= ~(1 << imm);
        }
    });

    root.Add(0x7, 0x3, {
        "BTST #xx:3, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.bH();
            const uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            cpu.reg.flags.Z = !(*rd & (1 << imm));
        }
    });


    root.Add(0x7, 0x7, {
        "BLD #xx:3, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.bH();
            const uint8_t* rd = cpu.reg.Reg8(cpu.bL());

            cpu.reg.flags.C = (*rd >> imm) & 1;
        }
    });

    root.AddSubtable(0x7, 0x9,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x79, 0x00, {
                "MOV.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = imm;
                    cpu.reg.MovFlags(imm);
                }
            });

            table.Add(0x79, 0x01, {
                "ADD.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.reg.Add(*rd, imm);
                }
            });

            table.Add(0x79, 0x02, {
                "CMP.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    const uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    cpu.reg.Sub(*rd, imm);
                }
            });

            table.Add(0x79, 0x03, {
                "SUB.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd = cpu.reg.Sub(*rd, imm);
                }
            });

            table.Add(0x79, 0x4, {
                "OR.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd |= imm;
                    cpu.reg.MovFlags(*rd);
                }
            });

            table.Add(0x79, 0x06, {
                "AND.W #xx:16, Rd",
                4,
                {2, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint16_t imm = cpu.cd();
                    uint16_t* rd = cpu.reg.Reg16(cpu.bL());

                    *rd &= imm;
                    cpu.reg.MovFlags(*rd);
                }
            });
        });


    root.AddSubtable(0x7, 0xA,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.a()); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.bH()); },
        [](InstructionTable& table)
        {
            table.Add(0x7A, 0x00, {
                "MOV.L #xx:32, ERd",
                6,
                {3, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t imm = (cpu.cd() << 16) | cpu.ef();
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());

                    *erd = imm;
                    cpu.reg.MovFlags(imm);
                }
            });

            table.Add(0x7A, 0x01, {
                "ADD.L #xx:32, ERd",
                6,
                {3, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t imm = (cpu.cd() << 16) | cpu.ef();
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());

                    *erd = cpu.reg.Add(*erd, imm);
                }
            });

            table.Add(0x7A, 0x02, {
                "CMP.L #xx:32, ERd",
                6,
                {3, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t imm = (cpu.cd() << 16) | cpu.ef();
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());

                    cpu.reg.Sub(*erd, imm);
                }
            });

            table.Add(0x7A, 0x06, {
                "AND.L #xx:32, ERd",
                6,
                {3, 0, 0, 0, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t imm = (cpu.cd() << 16) | cpu.ef();
                    uint32_t* erd = cpu.reg.Reg32(cpu.bL());

                    *erd &= imm;
                    cpu.reg.MovFlags(*erd);
                }
            });
        });

    root.AddSubtable(0x7, 0xC,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
        [](InstructionTable& table)
        {
            table.AddPattern(0x7C007, 0xFF0FF, 0x07, 0xFF, {
                "BLD #xx:3, @ERd",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t imm = cpu.dH();

                    const uint8_t value = cpu.mem->Read8(*erd);

                    cpu.reg.flags.C = value & (1 << imm);
                }
            });
        });

    root.AddSubtable(0x7, 0xD,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
        [](InstructionTable& table)
        {
            table.AddPattern(0x7D006, 0xFF0FF, 0x07, 0xFF, {
                "BST #xx:3, @ERd",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t imm = cpu.dH();

                    const uint8_t value = cpu.mem->Read8(*erd);

                    if (cpu.reg.flags.C)
                    {
                        cpu.mem->Write8(*erd, value | (1 << imm));
                    }
                    else
                    {
                        cpu.mem->Write8(*erd, value & ~(1 << imm));
                    }
                }
            });

            table.AddPattern(0x7D007, 0xFF0FF, 0x00, 0xFF, {
                "BSET #xx:3, @ERd",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t imm = cpu.dH();

                    cpu.mem->Write8(*erd, cpu.mem->Read8(*erd) | (1 << imm));
                }
            });

            table.AddPattern(0x7D007, 0xFF0FF, 0x01, 0xFF, {
                "BNOT #xx:3, @ERd",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t imm = cpu.dH();

                    cpu.mem->Write8(*erd, cpu.mem->Read8(*erd) ^ (1 << imm));
                }
            });

            table.AddPattern(0x7D007, 0xFF0FF, 0x02, 0xFF, {
                "BCLR #xx:3, @ERd",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint32_t* erd = cpu.reg.Reg32(cpu.bH());
                    const uint8_t imm = cpu.dH();

                    cpu.mem->Write8(*erd, cpu.mem->Read8(*erd) & ~(1 << imm));
                }
            });
        });


    root.AddSubtable(0x7, 0xE,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
        [](InstructionTable& table)
        {
            table.AddPattern(0x7E007, 0xFF00F, 0x07, 0xFF, {
                "BLD #xx:3, @aa:8",
                4,
                {2, 0, 0, 1, 0, 0},
                [](CPU& cpu)
                {
                    const uint8_t abs = cpu.b();
                    const uint8_t imm = cpu.dH();

                    const uint16_t address = 0xFF00 | abs;
                    const uint8_t value = cpu.mem->Read8(address);

                    cpu.reg.flags.C = value & (1 << imm);
                }
            });
        });


    root.AddSubtable(0x7, 0xF,
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.ab()) << 4 | cpu.cH(); },
        [](const CPU& cpu) { return static_cast<uint32_t>(cpu.cL()); },
        [](InstructionTable& table)
        {
            table.AddPattern(0x7F007, 0xFF00F, 0x00, 0xFF, {
                "BSET #xx:3, @aa:8",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint8_t abs = cpu.b();
                    const uint8_t imm = cpu.dH();

                    const uint16_t address = 0xFF00 | abs;

                    cpu.mem->Write8(address, cpu.mem->Read8(address) | (1 << imm));
                }
            });

            table.AddPattern(0x7F007, 0xFF00F, 0x02, 0xFF, {
                "BCLR #xx:3, @aa:8",
                4,
                {2, 0, 0, 2, 0, 0},
                [](CPU& cpu)
                {
                    const uint8_t abs = cpu.b();
                    const uint8_t imm = cpu.dH();

                    const uint16_t address = 0xFF00 | abs;

                    cpu.mem->Write8(address, cpu.mem->Read8(address) & ~(1 << imm));
                }
            });
        });

    root.Add(0x8, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "ADD.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            *rd = cpu.reg.Add(*rd, imm);
        }
    });

    root.Add(0xA, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "CMP.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            const uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            cpu.reg.Sub(*rd, imm);
        }
    });

    root.Add(0xC, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "OR.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            *rd |= imm;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0xD, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "XOR.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            *rd ^= imm;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0xE, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "AND.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            *rd &= imm;
            cpu.reg.MovFlags(*rd);
        }
    });

    root.Add(0xF, {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, {
        "MOV.B #xx:8, Rd",
        2,
        {1, 0, 0, 0, 0, 0},
        [](CPU& cpu)
        {
            const uint8_t imm = cpu.b();
            uint8_t* rd = cpu.reg.Reg8(cpu.aL());

            *rd = imm;
            cpu.reg.MovFlags(*rd);
        }
    });
}

const Instruction* InstructionSet::Decode(const CPU& cpu) const
{
    return root.Decode(cpu);
}
