-- simple_byte_pixel_counter_generator.vhd

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity simple_byte_pixel_counter_generator is
	generic (
		WIDTH          : integer := 640;
		HEIGHT         : integer := 480
	);
	port (
		data          : out std_logic_vector(7 downto 0);		-- avalon_streaming_source.data
		startofpacket : out std_logic;                          -- .startofpacket
		endofpacket   : out std_logic;                          -- .endofpacket
		valid         : out std_logic;                          -- .valid
		sink_ready    : in  std_logic := '0';
		clock_signal  : in  std_logic := '0'; 						-- clock.clk
		reset_n_signal  : in  std_logic := '0'  		 		-- reset_n.reset_n
	);
end entity simple_byte_pixel_counter_generator;

architecture rtl of simple_byte_pixel_counter_generator is
	-- TODO: ctrl_reg will get a proper Avalon MM slave write interface in a future revision
	-- signal ctrl_reg : std_logic_vector(7 downto 0) := (0 => '1', others => '0');
	-- ctrl_reg(0): enable - activate the modules.
	-- ctrl_reg(1): soft_res — software reset, behaves like hardware reset when set
	-- alias soft_res : std_logic is ctrl_reg(1);
begin

	process(clock_signal)
		variable pixel_counter : unsigned(31 downto 0) := (others => '0');
	begin
		if rising_edge(clock_signal) then
			-- TODO: restore "or soft_res = '1'" when ctrl_reg is writable
			if reset_n_signal = '0' then
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

					-- TODO: restore "if ctrl_reg(0) = '1' then" when ctrl_reg is writable
					data <= std_logic_vector(pixel_counter(7 downto 0));
					case to_integer(pixel_counter) is
						when 0 =>
							pixel_counter := pixel_counter + 1;
							startofpacket <= '1';
							valid         <= '1';
						when WIDTH * HEIGHT - 1 =>
							pixel_counter := (others => '0');
							endofpacket   <= '1';
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

end architecture rtl; -- of simple_byte_pixel_counter_generator
