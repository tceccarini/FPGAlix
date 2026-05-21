-- byte_pixel_counter_generator.vhd

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity byte_pixel_counter_generator is
	generic (
		WIDTH  : integer := 640;
		HEIGHT : integer := 480
	);
	port (
		st_data          : out std_logic_vector(7 downto 0);  -- avalon_streaming_source.data
		st_startofpacket : out std_logic;                     -- .startofpacket
		st_endofpacket   : out std_logic;                     -- .endofpacket
		st_valid         : out std_logic;                     -- .valid
		st_sink_ready    : in  std_logic := '0';              -- .ready

		mm_address    : in  std_logic_vector(2 downto 0)  := (others => '0'); -- avalon_slave.address
		mm_read_data  : out std_logic_vector(31 downto 0);                    -- .readdata
		mm_write_data : in  std_logic_vector(31 downto 0) := (others => '0'); -- .writedata
		mm_write      : in  std_logic                     := '0';             -- .write
		mm_read       : in  std_logic                     := '0';             -- .read

		clock_signal   : in std_logic := '0';  -- clock.clk
		reset_n_signal : in std_logic := '0'   -- reset_n.reset_n
	);
end entity byte_pixel_counter_generator;

architecture rtl of byte_pixel_counter_generator is
	signal x00_reg_control           : std_logic_vector(31 downto 0) := (others => '0');
	--   bit 0: enabled
	--   bit 1: clr_bpressure_evt_cnt (self-clearing)
	alias  ctrl_enabled          : std_logic is x00_reg_control(0);
	alias  clr_bpressure_evt_cnt : std_logic is x00_reg_control(1);

	signal x01_reg_bpressure_evt_cnt : unsigned(31 downto 0) := (others => '0');

begin

	-- main clocked process
	process(clock_signal)
		variable pixel_counter : unsigned(31 downto 0) := (others => '0');
	begin
		if rising_edge(clock_signal) then
			if reset_n_signal = '0' then
				st_valid              <= '0';
				st_data               <= (others => '0');
				st_startofpacket      <= '0';
				st_endofpacket        <= '0';
				
				x00_reg_control         <= (others => '0');
				x01_reg_bpressure_evt_cnt <= (others => '0');
				
				pixel_counter         := (others => '0');
			else

				-- Control logic ---------------------------------------------------------
				-- MM Write handling
				if mm_write = '1' then
					case to_integer(unsigned(mm_address)) is
						when 0 =>
							x00_reg_control <= mm_write_data;
						-- no other writable registers			
						when others =>
							null;
					end case;
				end if;		
				-- MM Read handling
				if mm_read = '1' then
					case to_integer(unsigned(mm_address)) is
						when 0 =>
							mm_read_data <= x00_reg_control;
						when 1 =>
							mm_read_data <= std_logic_vector(x01_reg_bpressure_evt_cnt);
						-- no other readable registers			
						when others =>
							mm_read_data <= (others => '0');
					end case;
				end if;

			
				-- streaming logic
				if enabled = '1' then
					if st_sink_ready = '1' then
						st_startofpacket <= '0';
						st_endofpacket   <= '0';
						st_valid         <= '0';

						st_data <= std_logic_vector(pixel_counter(7 downto 0));
						case to_integer(pixel_counter) is
							when 0 =>
								pixel_counter    := pixel_counter + 1;
								st_startofpacket <= '1';
								st_valid         <= '1';
							when WIDTH * HEIGHT - 1 =>
								pixel_counter  := (others => '0');
								st_endofpacket <= '1';
								st_valid       <= '1';
							when others =>
								pixel_counter := pixel_counter + 1;
								st_valid      <= '1';
						end case;
					end if;
					-- st_sink_ready = '0': outputs hold, backpressure counted above
				else
					st_valid         <= '0';
					st_startofpacket <= '0';
					st_endofpacket   <= '0';
				end if;
			end if;
		end if;
	end process;

end architecture rtl;
