library ieee;
library soc_system;


use ieee.std_logic_1164.all;



entity FPGAlix is
    port(
--        -- ADC
--        ADC_CS_n : out std_logic;
--        ADC_DIN  : out std_logic;
--        ADC_DOUT : in  std_logic;
--        ADC_SCLK : out std_logic;
--
--        -- Audio
--        AUD_ADCDAT  : in    std_logic;
--        AUD_ADCLRCK : inout std_logic;
--        AUD_BCLK    : inout std_logic;
--        AUD_DACDAT  : out   std_logic;
--        AUD_DACLRCK : inout std_logic;
--        AUD_XCK     : out   std_logic;

        -- CLOCK
        CLOCK_50  : in std_logic;
        CLOCK2_50 : in std_logic;
        CLOCK3_50 : in std_logic;
        CLOCK4_50 : in std_logic;

--        -- SDRAM
--        DRAM_ADDR  : out   std_logic_vector(12 downto 0);
--        DRAM_BA    : out   std_logic_vector(1 downto 0);
--        DRAM_CAS_N : out   std_logic;
--        DRAM_CKE   : out   std_logic;
--        DRAM_CLK   : out   std_logic;
--        DRAM_CS_N  : out   std_logic;
--        DRAM_DQ    : inout std_logic_vector(15 downto 0);
--        DRAM_LDQM  : out   std_logic;
--        DRAM_RAS_N : out   std_logic;
--        DRAM_UDQM  : out   std_logic;
--        DRAM_WE_N  : out   std_logic;

--        -- I2C for Audio and Video-In
--        FPGA_I2C_SCLK : out   std_logic;
--        FPGA_I2C_SDAT : inout std_logic;

--        -- SEG7
        HEX0_N : out std_logic_vector(6 downto 0);
        HEX1_N : out std_logic_vector(6 downto 0);
        HEX2_N : out std_logic_vector(6 downto 0);
        HEX3_N : out std_logic_vector(6 downto 0);
        HEX4_N : out std_logic_vector(6 downto 0);
        HEX5_N : out std_logic_vector(6 downto 0);

--        -- IR
--        IRDA_RXD : in  std_logic;
--        IRDA_TXD : out std_logic;

        -- KEY_N
        KEY_N : in std_logic_vector(3 downto 0);

        -- LED
        LEDR : out std_logic_vector(9 downto 0);

--        -- PS2
--        PS2_CLK  : inout std_logic;
--        PS2_CLK2 : inout std_logic;
--        PS2_DAT  : inout std_logic;
--        PS2_DAT2 : inout std_logic;

        -- SW
        SW : in std_logic_vector(9 downto 0);

--        -- Video-In
--        TD_CLK27   : inout std_logic;
--        TD_DATA    : out   std_logic_vector(7 downto 0);
--        TD_HS      : out   std_logic;
--        TD_RESET_N : out   std_logic;
--        TD_VS      : out   std_logic;
--
--        -- VGA
--        VGA_B       : out std_logic_vector(7 downto 0);
--        VGA_BLANK_N : out std_logic;
--        VGA_CLK     : out std_logic;
--        VGA_G       : out std_logic_vector(7 downto 0);
--        VGA_HS      : out std_logic;
--        VGA_R       : out std_logic_vector(7 downto 0);
--        VGA_SYNC_N  : out std_logic;
--        VGA_VS      : out std_logic;

        -- GPIO_0 (camera interface)
        GPIO_0 : inout std_logic_vector(35 downto 0);
--
--        -- GPIO_1
--        GPIO_1 : inout std_logic_vector(35 downto 0);

        -- HPS
        HPS_CONV_USB_N   : inout std_logic;
        HPS_DDR3_ADDR    : out   std_logic_vector(14 downto 0);
        HPS_DDR3_BA      : out   std_logic_vector(2 downto 0);
        HPS_DDR3_CAS_N   : out   std_logic;
        HPS_DDR3_CK_N    : out   std_logic;
        HPS_DDR3_CK_P    : out   std_logic;
        HPS_DDR3_CKE     : out   std_logic;
        HPS_DDR3_CS_N    : out   std_logic;
        HPS_DDR3_DM      : out   std_logic_vector(3 downto 0);
        HPS_DDR3_DQ      : inout std_logic_vector(31 downto 0);
        HPS_DDR3_DQS_N   : inout std_logic_vector(3 downto 0);
        HPS_DDR3_DQS_P   : inout std_logic_vector(3 downto 0);
        HPS_DDR3_ODT     : out   std_logic;
        HPS_DDR3_RAS_N   : out   std_logic;
        HPS_DDR3_RESET_N : out   std_logic;
        HPS_DDR3_RZQ     : in    std_logic;
        HPS_DDR3_WE_N    : out   std_logic;
        HPS_ENET_GTX_CLK : out   std_logic;
        HPS_ENET_INT_N   : inout std_logic;
        HPS_ENET_MDC     : out   std_logic;
        HPS_ENET_MDIO    : inout std_logic;
        HPS_ENET_RX_CLK  : in    std_logic;
        HPS_ENET_RX_DATA : in    std_logic_vector(3 downto 0);
        HPS_ENET_RX_DV   : in    std_logic;
        HPS_ENET_TX_DATA : out   std_logic_vector(3 downto 0);
        HPS_ENET_TX_EN   : out   std_logic;
        HPS_FLASH_DATA   : inout std_logic_vector(3 downto 0);
        HPS_FLASH_DCLK   : out   std_logic;
        HPS_FLASH_NCSO   : out   std_logic;
        HPS_GSENSOR_INT  : inout std_logic;
        HPS_I2C_CONTROL  : inout std_logic;
        HPS_I2C1_SCLK    : inout std_logic;
        HPS_I2C1_SDAT    : inout std_logic;
        HPS_I2C2_SCLK    : inout std_logic;
        HPS_I2C2_SDAT    : inout std_logic;
        HPS_KEY_N        : inout std_logic;
        HPS_LED          : inout std_logic;
        HPS_LTC_GPIO     : inout std_logic;
        HPS_SD_CLK       : out   std_logic;
        HPS_SD_CMD       : inout std_logic;
        HPS_SD_DATA      : inout std_logic_vector(3 downto 0);
        HPS_SPIM_CLK     : out   std_logic;
        HPS_SPIM_MISO    : in    std_logic;
        HPS_SPIM_MOSI    : out   std_logic;
        HPS_SPIM_SS      : inout std_logic;
        HPS_UART_RX      : in    std_logic;
        HPS_UART_TX      : out   std_logic;
        HPS_USB_CLKOUT   : in    std_logic;
        HPS_USB_DATA     : inout std_logic_vector(7 downto 0);
        HPS_USB_DIR      : in    std_logic;
        HPS_USB_NXT      : in    std_logic;
        HPS_USB_STP      : out   std_logic
    );
end entity FPGAlix;

architecture rtl of FPGAlix is
    signal hps_fpga_reset_n   	 	: std_logic;
    signal hps_reset_req      	 	: std_logic_vector(2 downto 0);
    signal hps_cold_reset     	 	: std_logic;
    signal hps_warm_reset     	 	: std_logic;
    signal hps_debug_reset    	 	: std_logic;
    signal hps_f2h_cold_n     	 	: std_logic;
    signal hps_f2h_warm_n     		: std_logic;
    signal hps_f2h_debug_n    	   : std_logic;
    signal stm_hw_events            : std_logic_vector(27 downto 0);
    signal ov7670_ctrl_if_sda_oe    : std_logic;
    signal ov7670_ctrl_if_scl_oe    : std_logic;
    signal ov7670_data_if_data      : std_logic_vector(7 downto 0);
    signal fpga_led_internal   		: std_logic_vector(9 downto 0);
    signal cold_prev           		: std_logic;
    signal warm_prev           		: std_logic;
    signal debug_prev          		: std_logic;
    signal cold_cnt            		: integer range 0 to 6;
    signal warm_cnt            		: integer range 0 to 2;
    signal debug_cnt           		: integer range 0 to 32;

begin
	soc_system_inst : entity soc_system.soc_system
		port map(
			clk_clk => CLOCK_50,
			fpga_button_pio_external_connection_export => KEY_N,
			fpga_dipsw_pio_external_connection_export  => SW,
			fpga_led_pio_external_connection_export    => fpga_led_internal,
			hps_ddr3_mem_a => HPS_DDR3_ADDR,
			hps_ddr3_mem_ba => HPS_DDR3_BA,
			hps_ddr3_mem_ck => HPS_DDR3_CK_P,
			hps_ddr3_mem_ck_n => HPS_DDR3_CK_N,
			hps_ddr3_mem_cke => HPS_DDR3_CKE,
			hps_ddr3_mem_cs_n => HPS_DDR3_CS_N,
			hps_ddr3_mem_ras_n => HPS_DDR3_RAS_N,
			hps_ddr3_mem_cas_n => HPS_DDR3_CAS_N,
			hps_ddr3_mem_we_n => HPS_DDR3_WE_N,
			hps_ddr3_mem_reset_n => HPS_DDR3_RESET_N,
			hps_ddr3_mem_dq => HPS_DDR3_DQ,
			hps_ddr3_mem_dqs => HPS_DDR3_DQS_P,
			hps_ddr3_mem_dqs_n => HPS_DDR3_DQS_N,
			hps_ddr3_mem_odt => HPS_DDR3_ODT,
			hps_ddr3_mem_dm => HPS_DDR3_DM,
			hps_ddr3_oct_rzqin => HPS_DDR3_RZQ,
			hps_io_hps_io_emac1_inst_TX_CLK => HPS_ENET_GTX_CLK,
			hps_io_hps_io_emac1_inst_TX_CTL => HPS_ENET_TX_EN,
			hps_io_hps_io_emac1_inst_TXD0 => HPS_ENET_TX_DATA(0),
			hps_io_hps_io_emac1_inst_TXD1 => HPS_ENET_TX_DATA(1),
			hps_io_hps_io_emac1_inst_TXD2 => HPS_ENET_TX_DATA(2),
			hps_io_hps_io_emac1_inst_TXD3 => HPS_ENET_TX_DATA(3),
			hps_io_hps_io_emac1_inst_RX_CLK => HPS_ENET_RX_CLK,
			hps_io_hps_io_emac1_inst_RX_CTL => HPS_ENET_RX_DV,
			hps_io_hps_io_emac1_inst_RXD0 => HPS_ENET_RX_DATA(0),
			hps_io_hps_io_emac1_inst_RXD1 => HPS_ENET_RX_DATA(1),
			hps_io_hps_io_emac1_inst_RXD2 => HPS_ENET_RX_DATA(2),
			hps_io_hps_io_emac1_inst_RXD3 => HPS_ENET_RX_DATA(3),
			hps_io_hps_io_emac1_inst_MDIO => HPS_ENET_MDIO,
			hps_io_hps_io_emac1_inst_MDC => HPS_ENET_MDC,
			hps_io_hps_io_qspi_inst_CLK => HPS_FLASH_DCLK,
			hps_io_hps_io_qspi_inst_SS0 => HPS_FLASH_NCSO,
			hps_io_hps_io_qspi_inst_IO0 => HPS_FLASH_DATA(0),
			hps_io_hps_io_qspi_inst_IO1 => HPS_FLASH_DATA(1),
			hps_io_hps_io_qspi_inst_IO2 => HPS_FLASH_DATA(2),
			hps_io_hps_io_qspi_inst_IO3 => HPS_FLASH_DATA(3),
			hps_io_hps_io_sdio_inst_CLK => HPS_SD_CLK,
			hps_io_hps_io_sdio_inst_CMD => HPS_SD_CMD,
			hps_io_hps_io_sdio_inst_D0 => HPS_SD_DATA(0),
			hps_io_hps_io_sdio_inst_D1 => HPS_SD_DATA(1),
			hps_io_hps_io_sdio_inst_D2 => HPS_SD_DATA(2),
			hps_io_hps_io_sdio_inst_D3 => HPS_SD_DATA(3),
			hps_io_hps_io_usb1_inst_CLK => HPS_USB_CLKOUT,
			hps_io_hps_io_usb1_inst_STP => HPS_USB_STP,
			hps_io_hps_io_usb1_inst_DIR => HPS_USB_DIR,
			hps_io_hps_io_usb1_inst_NXT => HPS_USB_NXT,
			hps_io_hps_io_usb1_inst_D0 => HPS_USB_DATA(0),
			hps_io_hps_io_usb1_inst_D1 => HPS_USB_DATA(1),
			hps_io_hps_io_usb1_inst_D2 => HPS_USB_DATA(2),
			hps_io_hps_io_usb1_inst_D3 => HPS_USB_DATA(3),
			hps_io_hps_io_usb1_inst_D4 => HPS_USB_DATA(4),
			hps_io_hps_io_usb1_inst_D5 => HPS_USB_DATA(5),
			hps_io_hps_io_usb1_inst_D6 => HPS_USB_DATA(6),
			hps_io_hps_io_usb1_inst_D7 => HPS_USB_DATA(7),
			hps_io_hps_io_spim1_inst_CLK => HPS_SPIM_CLK,
			hps_io_hps_io_spim1_inst_MOSI => HPS_SPIM_MOSI,
			hps_io_hps_io_spim1_inst_MISO => HPS_SPIM_MISO,
			hps_io_hps_io_spim1_inst_SS0 => HPS_SPIM_SS,
			hps_io_hps_io_uart0_inst_RX => HPS_UART_RX,
			hps_io_hps_io_uart0_inst_TX => HPS_UART_TX,
			hps_io_hps_io_i2c0_inst_SDA => HPS_I2C1_SDAT,
			hps_io_hps_io_i2c0_inst_SCL => HPS_I2C1_SCLK,
			hps_io_hps_io_i2c1_inst_SDA => HPS_I2C2_SDAT,
			hps_io_hps_io_i2c1_inst_SCL => HPS_I2C2_SCLK,
			hps_io_hps_io_gpio_inst_GPIO09 => HPS_CONV_USB_N,
			hps_io_hps_io_gpio_inst_GPIO35 => HPS_ENET_INT_N,
			hps_io_hps_io_gpio_inst_GPIO40 => HPS_LTC_GPIO,
			hps_io_hps_io_gpio_inst_GPIO48 => HPS_I2C_CONTROL,
			hps_io_hps_io_gpio_inst_GPIO53 => HPS_LED,
			hps_io_hps_io_gpio_inst_GPIO54 => HPS_KEY_N,
			hps_io_hps_io_gpio_inst_GPIO61 => HPS_GSENSOR_INT,
			hps_f2h_cold_reset_req_reset_n             => hps_f2h_cold_n,
			hps_f2h_debug_reset_req_reset_n            => hps_f2h_debug_n,
			hps_f2h_warm_reset_req_reset_n             => hps_f2h_warm_n,
			hps_f2h_stm_hw_events_stm_hwevents         => stm_hw_events,
			hps_h2f_reset_reset_n                      => hps_fpga_reset_n,
			issp_hps_resets_source                     => hps_reset_req,
			reset_reset_n => '1',
			hex_dsplay_hex0_n => HEX0_N,
			hex_dsplay_hex1_n => HEX1_N,
			hex_dsplay_hex2_n => HEX2_N,
			hex_dsplay_hex3_n => HEX3_N,
			hex_dsplay_hex4_n => HEX4_N,
			hex_dsplay_hex5_n                          => HEX5_N,
			ov7670_pclk_clk                            => GPIO_0(1),
			ov7670_ctrl_if_sda_in                      => GPIO_0(27),
			ov7670_ctrl_if_scl_in                      => GPIO_0(29),
			ov7670_ctrl_if_sda_oe                      => ov7670_ctrl_if_sda_oe,
			ov7670_ctrl_if_scl_oe                      => ov7670_ctrl_if_scl_oe,
			ov7670_reset_n_camera_reset_n              => GPIO_0(31),
			ov7670_data_if_cam_pwdn                    => GPIO_0(33),
			ov7670_data_if_cam_href                    => GPIO_0(3),
			ov7670_data_if_cam_vsync                   => GPIO_0(5),
			ov7670_data_if_cam_data                    => ov7670_data_if_data
		);


    -- I2C open-drain (active low drive)
    GPIO_0(27) <= '0' when ov7670_ctrl_if_sda_oe = '1' else 'Z';
    GPIO_0(29) <= '0' when ov7670_ctrl_if_scl_oe = '1' else 'Z';

    -- CAM data bus (non-contiguous GPIO pins)
    ov7670_data_if_data(0) <= GPIO_0(11);
    ov7670_data_if_data(1) <= GPIO_0(13);
    ov7670_data_if_data(2) <= GPIO_0(15);
    ov7670_data_if_data(3) <= GPIO_0(17);
    ov7670_data_if_data(4) <= GPIO_0(19);
    ov7670_data_if_data(5) <= GPIO_0(21);
    ov7670_data_if_data(6) <= GPIO_0(23);
    ov7670_data_if_data(7) <= GPIO_0(25);

    -- LED routing e STM events
    LEDR          <= fpga_led_internal;
    stm_hw_events <= "0000" & SW & fpga_led_internal & KEY_N;

    -- Inversione reset F2H (attivi bassi)
    hps_f2h_cold_n  <= not hps_cold_reset;
    hps_f2h_debug_n <= not hps_debug_reset;
    hps_f2h_warm_n  <= not hps_warm_reset;

    -- Edge detector cold reset (PULSE_EXT=6)
    process(CLOCK_50, hps_fpga_reset_n)
    begin
        if hps_fpga_reset_n = '0' then
            cold_prev      <= '0';
            cold_cnt       <= 0;
            hps_cold_reset <= '0';
        elsif rising_edge(CLOCK_50) then
            cold_prev <= hps_reset_req(0);
            if hps_reset_req(0) = '1' and cold_prev = '0' and cold_cnt = 0 then
                cold_cnt <= 6;
            elsif cold_cnt > 0 then
                cold_cnt <= cold_cnt - 1;
            end if;
            if cold_cnt > 0 or (hps_reset_req(0) = '1' and cold_prev = '0') then
                hps_cold_reset <= '1';
            else
                hps_cold_reset <= '0';
            end if;
        end if;
    end process;

    -- Edge detector warm reset (PULSE_EXT=2)
    process(CLOCK_50, hps_fpga_reset_n)
    begin
        if hps_fpga_reset_n = '0' then
            warm_prev      <= '0';
            warm_cnt       <= 0;
            hps_warm_reset <= '0';
        elsif rising_edge(CLOCK_50) then
            warm_prev <= hps_reset_req(1);
            if hps_reset_req(1) = '1' and warm_prev = '0' and warm_cnt = 0 then
                warm_cnt <= 2;
            elsif warm_cnt > 0 then
                warm_cnt <= warm_cnt - 1;
            end if;
            if warm_cnt > 0 or (hps_reset_req(1) = '1' and warm_prev = '0') then
                hps_warm_reset <= '1';
            else
                hps_warm_reset <= '0';
            end if;
        end if;
    end process;

    -- Edge detector debug reset (PULSE_EXT=32)
    process(CLOCK_50, hps_fpga_reset_n)
    begin
        if hps_fpga_reset_n = '0' then
            debug_prev      <= '0';
            debug_cnt       <= 0;
            hps_debug_reset <= '0';
        elsif rising_edge(CLOCK_50) then
            debug_prev <= hps_reset_req(2);
            if hps_reset_req(2) = '1' and debug_prev = '0' and debug_cnt = 0 then
                debug_cnt <= 32;
            elsif debug_cnt > 0 then
                debug_cnt <= debug_cnt - 1;
            end if;
            if debug_cnt > 0 or (hps_reset_req(2) = '1' and debug_prev = '0') then
                hps_debug_reset <= '1';
            else
                hps_debug_reset <= '0';
            end if;
        end if;
    end process;

end architecture rtl;