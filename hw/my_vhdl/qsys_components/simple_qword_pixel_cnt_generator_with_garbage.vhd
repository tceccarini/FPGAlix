-- simple_qword_pixel_cnt_generator_with_garbage.vhd

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity simple_qword_pixel_cnt_generator_with_garbage is
	generic (
		WIDTH         : integer := 640;
		HEIGHT        : integer := 480;
		GARBAGE_QWORDS : integer := 4
	);
	port (
		data          : out std_logic_vector(63 downto 0);        -- st_source.data
		startofpacket : out std_logic;                            --          .startofpacket
		endofpacket   : out std_logic;                            --          .endofpacket
		valid         : out std_logic;                            --          .valid
		sink_ready    : in  std_logic := '0';                     --          .ready
		clock         : in  std_logic := '0';                     -- clock.clk
		reset_n       : in  std_logic := '0'                      -- reset.reset_n
	);
end entity simple_qword_pixel_cnt_generator_with_garbage;

architecture rtl of simple_qword_pixel_cnt_generator_with_garbage is
begin

	process(clock)
		-- counts pixels + garbage beats; pixel frame = 0..WIDTH*HEIGHT-1,
		-- garbage region = WIDTH*HEIGHT..WIDTH*HEIGHT+GARBAGE_QWORDS-1
		variable pixel_counter : unsigned(63 downto 0) := (others => '0');
	begin
		if rising_edge(clock) then
			if reset_n = '0' then
				valid         <= '0';
				data          <= (others => '0');
				startofpacket <= '0';
				endofpacket   <= '0';
				pixel_counter := (others => '0');
			else
				if sink_ready = '1' then
					startofpacket <= '0';
					endofpacket   <= '0';
					valid         <= '0';
					data          <= std_logic_vector(pixel_counter);

					case to_integer(pixel_counter) is
						when 0 =>
							pixel_counter := pixel_counter + 1;
							startofpacket <= '1';
							valid         <= '1';
						when WIDTH * HEIGHT - 1 =>
							-- EOP: advance into garbage region
							pixel_counter := pixel_counter + 1;
							endofpacket   <= '1';
							valid         <= '1';
						when WIDTH * HEIGHT to WIDTH * HEIGHT + GARBAGE_QWORDS - 1 =>
							-- garbage beats: all-F, no SOP/EOP
							pixel_counter := pixel_counter + 1;
							data          <= (others => '1');
							valid         <= '1';
						when WIDTH * HEIGHT + GARBAGE_QWORDS =>
							-- end of garbage: wrap and emit SOP
							pixel_counter := to_unsigned(1, 64);
							data          <= (others => '0');
							startofpacket <= '1';
							valid         <= '1';
						when others =>
							pixel_counter := pixel_counter + 1;
							valid         <= '1';
					end case;
				end if;
				-- when sink_ready = '0': outputs hold their registered values
			end if;
		end if;
	end process;

end architecture rtl; -- of simple_qword_pixel_cnt_generator_with_garbage
