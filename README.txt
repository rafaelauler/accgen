
-- Current limitations

0) Code is hardcoded for big endian machines!

1)
When the format definition has groups like:
ac_format Type_I2   = "%op:5 [ %cond:2 %an:1 %simm8:8:s | %op2:2 %t:1 %reg32:5 %reg8:3 | %op3:2 %simm9:9:s ]";
its possible to parse, but for more than one group the behavior is undefined, suppose:
ac_format Type_I2   = "%op:5 [ %cond:2 %an:1 %simm8:8:s | %op2:2 %t:1 %reg32:5 %reg8:3 ] %op6:2 [ %op3:2 | %simm9:9:s ]";
that wont be parsed.
