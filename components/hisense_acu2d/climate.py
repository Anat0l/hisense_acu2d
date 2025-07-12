from esphome.components import climate
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, remote_transmitter
from esphome.components.climate import ClimateSwingMode
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_SUPPORTED_SWING_MODES

DEPENDENCIES = ["uart"]

hisense_acu2d_ns = cg.esphome_ns.namespace("hisense_acu2d")
HisenseACU2D = hisense_acu2d_ns.class_(
    "HisenseACU2D", cg.Component, climate.Climate, uart.UARTDevice
)

ALLOWED_CLIMATE_SWING_MODES = {
    "OFF" : ClimateSwingMode.CLIMATE_SWING_OFF,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

validate_swing_modes = cv.enum(ALLOWED_CLIMATE_SWING_MODES, upper=True)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(HisenseACU2D)
    .extend(
        {
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                validate_swing_modes
            ),
            cv.OnlyWith(CONF_TRANSMITTER_ID, "remote_transmitter"): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    ,
)

async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    if CONF_TRANSMITTER_ID in config:
        cg.add_define("USE_REMOTE_TRANSMITTER")
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
    
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
