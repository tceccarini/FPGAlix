library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ov7670_data_interface is
    generic (
        FRAME_SIZE : natural := 640*480  -- bytes per frame; multiply by 2 for RGB565
    );
    port (
        pclk            : in  std_logic;
        reset_n         : in  std_logic;

        -- OV7670 camera interface
        cam_pwdn        : out std_logic                     := '0';
        cam_href        : in  std_logic;
        cam_vsync       : in  std_logic;
        cam_data        : in  std_logic_vector(7 downto 0);

        -- Avalon-ST source (8-bit, with frame and backpressure)
        st_data         : out std_logic_vector(7 downto 0)  := (others => '0');
        st_valid        : out std_logic                     := '0';
        st_sop          : out std_logic                     := '0';
        st_eop          : out std_logic                     := '0';
        st_ready        : in  std_logic;

        -- Avalon-MM slave (32-bit data, 1-bit address)
        -- 0x0: ctrl (R/W)   0x1: stat_dropped_cnt (R)
        mm_address      : in  std_logic_vector(0 downto 0)  := (others => '0');
        mm_data_out     : out std_logic_vector(31 downto 0) := (others => '0');
        mm_data_in      : in  std_logic_vector(31 downto 0) := (others => '0');
        mm_write        : in  std_logic                     := '0';
        mm_read         : in  std_logic                     := '0'
    );
end entity ov7670_data_interface;

architecture rtl of ov7670_data_interface is

    type state_t is (WAIT_SYNC, ACQUIRE, WAIT_READY);
    signal state : state_t := WAIT_SYNC;

    -- ctrl register bit map (R/W at MM address 0x0):
    --   bit 0: ctrl_enabled        - enable frame acquisition
    --   bit 1: ctrl_clear_counters - self-clearing, clears stat_dropped_cnt
    --   bit 2: ctrl_cam_pwdn       - forwarded to cam_pwdn pin
    signal ctrl                : std_logic_vector(31 downto 0) := (others => '0');
    alias  ctrl_enabled        : std_logic is ctrl(0);
    alias  ctrl_clear_counters : std_logic is ctrl(1);
    alias  ctrl_cam_pwdn       : std_logic is ctrl(2);

    signal stat_byte_cnt       : std_logic_vector(31 downto 0) := (others => '0');
    signal stat_dropped_cnt    : std_logic_vector(31 downto 0) := (others => '0');

    signal vsync_r             : std_logic := '0';  -- previous cycle vsync, used for falling edge detection

begin

    cam_pwdn    <= ctrl_cam_pwdn;

    -- Avalon-MM process
    process(pclk)
    begin
        if rising_edge(pclk) then
            if reset_n = '0' then
                ctrl        <= (others => '0');
                mm_data_out <= (others => '0');
            else
                -- write
                if mm_write = '1' then
                    case mm_address is
                        when "0"    => ctrl <= mm_data_in;
                        when others => null;
                    end case;
                end if;
                -- read
                mm_data_out <= (others => '0');
                if mm_read = '1' then
                    case mm_address is
                        when "0"    => mm_data_out <= ctrl;
                        when "1"    => mm_data_out <= stat_dropped_cnt;
                        when others => null;
                    end case;
                end if;
                -- auto-clear ctrl_clear_counters after one cycle
                if ctrl_clear_counters = '1' then
                    ctrl_clear_counters <= '0';
                end if;
            end if;
        end if;
    end process;

    -- FSM process
    process(pclk)
    begin
        if rising_edge(pclk) then
            if reset_n = '0' then
                stat_byte_cnt    <= (others => '0');
                stat_dropped_cnt <= (others => '0');
                st_data          <= (others => '0');
                st_valid         <= '0';
                st_sop           <= '0';
                st_eop           <= '0';
                state            <= WAIT_SYNC;
                vsync_r          <= '0';
            else
                vsync_r <= cam_vsync;
                case state is

                    when WAIT_SYNC =>
                        st_valid         <= '0';
                        st_sop           <= '0';
                        st_eop           <= '0';
                        stat_byte_cnt    <= (others => '0');
                        if vsync_r = '1' and cam_vsync = '0' and ctrl_enabled = '1' then
                            state <= ACQUIRE;
                        end if;

                    when ACQUIRE =>
                        st_valid <= '0';
                        st_sop   <= '0';
                        st_eop   <= '0';
                        if st_ready = '0' then
                            -- backpressure: drop the frame, wait for ready
                            stat_dropped_cnt <= std_logic_vector(unsigned(stat_dropped_cnt) + 1);
                            state            <= WAIT_READY;
                        elsif cam_href = '1' then
                            st_valid      <= '1';
                            st_data       <= cam_data;
                            stat_byte_cnt <= std_logic_vector(unsigned(stat_byte_cnt) + 1);
                            -- start: first byte, raise sop
                            if unsigned(stat_byte_cnt) = 0 then
                                st_sop <= '1';
                            end if;
                            -- end: last byte, raise eop
                            if unsigned(stat_byte_cnt) = to_unsigned(FRAME_SIZE - 1, 32) then
                                st_eop <= '1';
                                state  <= WAIT_SYNC;
                            end if;
                            -- running: middle bytes, no sop/eop
                        end if;

                    when WAIT_READY =>
                        st_valid <= '0';
                        st_sop   <= '0';
                        st_eop   <= '0';
                        if st_ready = '1' then
                            -- close the dropped packet with eop, then resync
                            st_valid <= '1';
                            st_eop   <= '1';
                            state    <= WAIT_SYNC;
                        end if;

                end case;
                -- placed after case to take priority over any increment in the same cycle
                if ctrl_clear_counters = '1' then
                    stat_dropped_cnt <= (others => '0');
                end if;
            end if;
        end if;
    end process;

end architecture rtl;
