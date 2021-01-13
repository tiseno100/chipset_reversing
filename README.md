# chipset_reversing

A collection of reverse engineered chipsets for 86Box. Mostly simplistic glue logic chipsets produced around the 386 era.

These chipsets are nowhere near the quality of a documented chipset or function as intended. I'm personally not that experienced in C/C++ programming and emulators too in general. They're done mostly for leisure and learning.

Status of the chipsets
Chipset|File|Status|Info
-|-|-|-
Micronics MIC 471(486)|mic471.c|Paused|For now it doesn't work at all. It's shadowing procedure requires more understanding.
Macronix MXIC 307(386/486)|mxic307.c|Complete|Works fine with MR and AMI boards.
ALi ALADDiN III(Pentium)|ali_aladdin_iii.c|Maintained|Mixed functioncality. Still fairly incomplete. Apparently it has a datasheet.
UMC 491|umc491.c|Complete|Works fine with dozens of boards.
Winbond W8375X|hdc_ide_w8375x.c|Complete|Not really a chipset. It's a combo IDE controller used on many undocumented Winbond chipset motherboards.

__Potentially upcoming Chipsets__
- ALi M1419(386)
- PC Chips 286(286)
- Symphony SL82C362(386/486?)

__These chipsets will NOT be included in any build of 86Box unless requested by the team and I have zero pleasure to port them on PCem or VARCem. Feel free to use them for any personal use you want as long as I am credited__
