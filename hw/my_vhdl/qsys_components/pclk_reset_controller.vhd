library ieee;
use ieee.std_logic_1164.all;

entity pclk_reset_controller is
    port (
        -- system clock domain
        sys_clk       : in  std_logic;
        sys_reset_n   : in  std_logic;

        -- Avalon-MM slave (system clock domain)
        -- bit0: sw_release - write 1 to release pclk reset, 0 to assert it
        mm_writedata  : in  std_logic_vector(31 downto 0) := (others => '0');
        mm_write      : in  std_logic := '0';

        -- pclk domain
        pclk_in       : in  std_logic;
        pclk_out      : out std_logic;
        pclk_reset_n  : out std_logic
    );
end entity pclk_reset_controller;

architecture rtl of pclk_reset_controller is

    signal sw_release     : std_logic := '0';
    signal reset_combined : std_logic;

    -- two-FF synchroniser: assert async, deassert sync to pclk
    signal sync_ff1       : std_logic := '0';
    signal sync_ff2       : std_logic := '0';

    attribute altera_attribute : string;
    attribute altera_attribute of sync_ff1 : signal is "-name SYNCHRONIZER_IDENTIFICATION FORCED";

begin

    -- MM register on system clock
    process(sys_clk)
    begin
        if rising_edge(sys_clk) then
            if sys_reset_n = '0' then
                sw_release <= '0';
            elsif mm_write = '1' then
                sw_release <= mm_writedata(0);
            end if;
        end if;
    end process;

    reset_combined <= sys_reset_n and sw_release;

    -- reset synchroniser clocked on pclk
    process(pclk_in, reset_combined)
    begin
        if reset_combined = '0' then
            sync_ff1 <= '0';
            sync_ff2 <= '0';
        elsif rising_edge(pclk_in) then
            sync_ff1 <= '1';
            sync_ff2 <= sync_ff1;
        end if;
    end process;

    pclk_out     <= pclk_in;  -- Quartus infers a global clock buffer here
    pclk_reset_n <= sync_ff2;

end architecture rtl;
