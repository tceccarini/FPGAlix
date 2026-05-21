library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- =============================================================================
-- Entities
-- =============================================================================

entity hex_display_controller is
    port (
        clk           : in  std_logic;
        reset_n       : in  std_logic;

        -- Avalon-MM slave (system clock domain)
        -- ctrl register layout:
        --   bit 31     : enabled (1 = display on, 0 = all blank)
        --   bit [30:20]: reserved
        --   bit [19:0] : value 0..999999 (>999999 shows "------")
        mm_writedata  : in  std_logic_vector(31 downto 0) := (others => '0');
        mm_readdata   : out std_logic_vector(31 downto 0) := (others => '0');
        mm_write      : in  std_logic                     := '0';
        mm_read       : in  std_logic                     := '0';

        -- 7-segment outputs, active low, HEX0 = rightmost digit
        hex0_n          : out std_logic_vector(6 downto 0);
        hex1_n          : out std_logic_vector(6 downto 0);
        hex2_n          : out std_logic_vector(6 downto 0);
        hex3_n          : out std_logic_vector(6 downto 0);
        hex4_n          : out std_logic_vector(6 downto 0);
        hex5_n          : out std_logic_vector(6 downto 0)
    );
end entity hex_display_controller;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Combinatorial binary-to-BCD converter (Double Dabble algorithm).
-- Converts a 20-bit unsigned integer (0..999999) to 6 BCD digits.
entity bin_to_bcd_6digit is
    port (
        bin  : in  unsigned(19 downto 0);
        bcd5 : out unsigned(3 downto 0);  -- hundred-thousands
        bcd4 : out unsigned(3 downto 0);  -- ten-thousands
        bcd3 : out unsigned(3 downto 0);  -- thousands
        bcd2 : out unsigned(3 downto 0);  -- hundreds
        bcd1 : out unsigned(3 downto 0);  -- tens
        bcd0 : out unsigned(3 downto 0)   -- units
    );
end entity bin_to_bcd_6digit;


-- =============================================================================
-- Architectures
-- =============================================================================

architecture rtl of hex_display_controller is

    constant SEG_BLANK : std_logic_vector(6 downto 0) := "1111111";
    constant SEG_DASH  : std_logic_vector(6 downto 0) := "0111111";  -- segment g only

    type seg_lut_t is array (0 to 9) of std_logic_vector(6 downto 0);
    constant SEG_LUT : seg_lut_t := (
        0 => "1000000",
        1 => "1111001",
        2 => "0100100",
        3 => "0110000",
        4 => "0011001",
        5 => "0010010",
        6 => "0000010",
        7 => "1111000",
        8 => "0000000",
        9 => "0010000"
    );

    signal ctrl     : std_logic_vector(31 downto 0) := (others => '0');
    alias  enabled  : std_logic is ctrl(31);

    signal overflow : std_logic;
    signal bcd5, bcd4, bcd3, bcd2, bcd1, bcd0 : unsigned(3 downto 0);

begin

    -- MM register
    process(clk)
    begin
        if rising_edge(clk) then
            if reset_n = '0' then
                ctrl <= (others => '0');
            elsif mm_write = '1' then
                ctrl <= mm_writedata;
            end if;
        end if;
    end process;

    mm_readdata <= ctrl when mm_read = '1' else (others => '0');

    overflow <= '1' when unsigned(ctrl(19 downto 0)) > 999999 else '0';

    bcd_conv: entity work.bin_to_bcd_6digit
        port map (
            bin  => unsigned(ctrl(19 downto 0)),
            bcd5 => bcd5,
            bcd4 => bcd4,
            bcd3 => bcd3,
            bcd2 => bcd2,
            bcd1 => bcd1,
            bcd0 => bcd0
        );

    -- output with leading zero suppression; bcd0 never blank (shows "0" for value=0)
    hex5_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else
            SEG_BLANK when bcd5 = 0                                           else SEG_LUT(to_integer(bcd5));

    hex4_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else
            SEG_BLANK when bcd5 = 0 and bcd4 = 0                             else SEG_LUT(to_integer(bcd4));

    hex3_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else
            SEG_BLANK when bcd5 = 0 and bcd4 = 0 and bcd3 = 0               else SEG_LUT(to_integer(bcd3));

    hex2_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else
            SEG_BLANK when bcd5 = 0 and bcd4 = 0 and bcd3 = 0 and bcd2 = 0 else SEG_LUT(to_integer(bcd2));

    hex1_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else
            SEG_BLANK when bcd5 = 0 and bcd4 = 0 and bcd3 = 0 and bcd2 = 0 and bcd1 = 0
                                                                              else SEG_LUT(to_integer(bcd1));

    hex0_n <= SEG_BLANK when enabled = '0' else
            SEG_DASH  when overflow = '1' else SEG_LUT(to_integer(bcd0));

end architecture rtl;


architecture rtl of bin_to_bcd_6digit is

    -- scratch layout: bits [43:20] = 6 BCD nibbles (bcd5 at MSB),
    --                 bits [19:0]  = binary input being shifted out
    type scratch_t is array (0 to 20) of std_logic_vector(43 downto 0);
    signal scratch : scratch_t;

    function dabble_stage(s : std_logic_vector(43 downto 0))
        return std_logic_vector
    is
        variable v : std_logic_vector(43 downto 0);
        variable n : unsigned(3 downto 0);
    begin
        v := s;
        for i in 0 to 5 loop
            n := unsigned(v(43 - i*4 downto 40 - i*4));
            if n >= 5 then
                v(43 - i*4 downto 40 - i*4) := std_logic_vector(n + 3);
            end if;
        end loop;
        return v(42 downto 0) & '0';
    end function;

begin

    scratch(0) <= (43 downto 20 => '0') & std_logic_vector(bin);

    gen_stages: for i in 0 to 19 generate
        scratch(i + 1) <= dabble_stage(scratch(i));
    end generate;

    bcd5 <= unsigned(scratch(20)(43 downto 40));
    bcd4 <= unsigned(scratch(20)(39 downto 36));
    bcd3 <= unsigned(scratch(20)(35 downto 32));
    bcd2 <= unsigned(scratch(20)(31 downto 28));
    bcd1 <= unsigned(scratch(20)(27 downto 24));
    bcd0 <= unsigned(scratch(20)(23 downto 20));

end architecture rtl;
