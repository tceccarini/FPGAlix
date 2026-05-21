library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity counter_source is
    generic (
        FRAME_BEATS   : integer := 38400;
        INITIAL_VALUE : integer := 0;
        INCREMENT     : integer := 1
    );
    port (
        avalon_streaming_source_data          : out std_logic_vector(63 downto 0);
        avalon_streaming_source_valid         : out std_logic;
        avalon_streaming_source_ready         : in  std_logic := '0';
        avalon_streaming_source_endofpacket   : out std_logic;
        avalon_streaming_source_startofpacket : out std_logic;
        clk                                   : in  std_logic := '0';
        reset_n                               : in  std_logic := '0'
    );
end entity counter_source;

architecture rtl of counter_source is
    signal beat_cnt : integer range 0 to FRAME_BEATS - 1 := 0;
    signal counter  : unsigned(63 downto 0) := to_unsigned(INITIAL_VALUE, 64);
begin
    avalon_streaming_source_valid         <= '1';
    avalon_streaming_source_data          <= std_logic_vector(counter);
    avalon_streaming_source_startofpacket <= '1' when beat_cnt = 0 else '0';
    avalon_streaming_source_endofpacket   <= '1' when beat_cnt = FRAME_BEATS - 1 else '0';

    process(clk, reset_n)
    begin
        if reset_n = '0' then
            beat_cnt <= 0;
            counter  <= to_unsigned(INITIAL_VALUE, 64);
        elsif rising_edge(clk) then
            if avalon_streaming_source_ready = '1' then
                counter <= counter + to_unsigned(INCREMENT, 64);
                if beat_cnt = FRAME_BEATS - 1 then
                    beat_cnt <= 0;
                else
                    beat_cnt <= beat_cnt + 1;
                end if;
            end if;
        end if;
    end process;
end architecture rtl;
