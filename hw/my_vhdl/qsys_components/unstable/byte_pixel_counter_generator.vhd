-- byte_pixel_counter_generator.vhd

-- This file was auto-generated as a prototype implementation of a module
-- created in component editor.  It ties off all outputs to ground and
-- ignores all inputs.  It needs to be edited to make it do something
-- useful.
-- 
-- This file will not be automatically regenerated.  You should check it in
-- to your version control system if you want to keep it.

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity byte_pixel_counter_generator is
	generic (
		WIDTH  : integer := 640;
		HEIGHT : integer := 480
	);
	port (
		st_data          : out std_logic_vector(7 downto 0);                     -- avalon_streaming_source.data
		st_startofpacket : out std_logic;                                        --                        .startofpacket
		st_endofpacket   : out std_logic;                                        --                        .endofpacket
		st_valid         : out std_logic;                                        --                        .valid
		st_sink_ready    : in  std_logic                     := '0';             --                        .ready
		clock_signal     : in  std_logic                     := '0';             --                   clock.clk
		reset_n_signal   : in  std_logic                     := '0';             --                 reset_n.reset_n
		mm_address       : in  std_logic_vector(2 downto 0)  := (others => '0'); --            avalon_slave.address
		mm_read_data     : out std_logic_vector(31 downto 0);                    --                        .readdata
		mm__write_data   : in  std_logic_vector(31 downto 0) := (others => '0'); --                        .writedata
		mm_write         : in  std_logic                     := '0';             --                        .write
		mm_read          : in  std_logic                     := '0'              --                        .read
	);
end entity byte_pixel_counter_generator;

architecture rtl of byte_pixel_counter_generator is
begin

	-- TODO: Auto-generated HDL template

	st_valid <= '0';

	st_data <= "00000000";

	st_startofpacket <= '0';

	st_endofpacket <= '0';

	mm_read_data <= "00000000000000000000000000000000";

end architecture rtl; -- of byte_pixel_counter_generator
