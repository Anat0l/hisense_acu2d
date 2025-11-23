from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import uart, remote_transmitter, sensor, switch
from esphome.components.climate import ClimateSwingMode
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_SUPPORTED_SWING_MODES 

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "switch"]
CODEOWNERS = ["@Anat0l"]

hisense_acu2d_ns = cg.esphome_ns.namespace("hisense_acu2d")
HisenseACU2D = hisense_acu2d_ns.class_(
    "HisenseACU2D", cg.Component, climate.Climate, uart.UARTDevice
)

HisenseACU2DSwitch = hisense_acu2d_ns.class_(
    "HisenseACU2DSwitch", switch.Switch, cg.Component
)

ALLOWED_CLIMATE_SWING_MODES = {
    "OFF" : ClimateSwingMode.CLIMATE_SWING_OFF,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

validate_swing_modes = cv.enum(ALLOWED_CLIMATE_SWING_MODES, upper=True)

CONF_IFEEL_SENSOR = "ifeel_sensor"
CONF_IFEEL_SWITCH = "ifeel_switch"

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(HisenseACU2D)
    .extend(
        {
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                validate_swing_modes
            ),
            cv.Optional(CONF_IFEEL_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_IFEEL_SWITCH): switch.switch_schema(HisenseACU2DSwitch),
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

    if sensor_id := config.get(CONF_IFEEL_SENSOR):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_sensor(sens))

    if switch_id := config.get(CONF_IFEEL_SWITCH):
        switch_ = await switch.new_switch(switch_id)
        await cg.register_component(switch_, config)
        cg.add(var.set_ifeel_switch(switch_))
