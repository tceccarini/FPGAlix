library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity const_pattern_source is
    generic (
        PATTERN     : std_logic_vector(63 downto 0) := x"AAAAAAAAAAAAAAAA";
        FRAME_BEATS : integer := 38400  -- 640x480 / 8 pixel per beat
    );
    port (
        clk     : in  std_logic;
        reset_n : in  std_logic;
        data    : out std_logic_vector(63 downto 0);
        valid   : out std_logic;
        ready   : in  std_logic;
        sop     : out std_logic;
        eop     : out std_logic
    );
end entity const_pattern_source;

architecture rtl of const_pattern_source is
    signal cnt : integer range 0 to FRAME_BEATS - 1 := 0;
begin
    data  <= PATTERN;
    valid <= '1';
    sop   <= '1' when cnt = 0 else '0';
    eop   <= '1' when cnt = FRAME_BEATS - 1 else '0';

    process(clk, reset_n)
    begin
        if reset_n = '0' then
            cnt <= 0;
        elsif rising_edge(clk) then
            if ready = '1' then
                if cnt = FRAME_BEATS - 1 then
                    cnt <= 0;
                else
                    cnt <= cnt + 1;
                end if;
            end if;
        end if;
    end process;
end architecture rtl;
