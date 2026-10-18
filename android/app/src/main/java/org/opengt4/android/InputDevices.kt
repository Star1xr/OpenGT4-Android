package org.opengt4.android

import android.content.Context
import android.hardware.input.InputManager
import android.view.InputDevice

/**
 * Device kinds, the same bits the native side takes (PLAT_INPUT_DEVICE_* in
 * include/platform/platform.h).
 */
object DeviceKind {
    const val GAMEPAD = 1
    const val KEYBOARD = 2
    const val MOUSE = 4
}

/** One device the system reports, in the terms the on-screen pad policy uses. */
class ConnectedDevice(val id: Int, val name: String, val kind: Int)

/**
 * What the launcher wants the on-screen pad to do.
 */
enum class TouchMode(val id: String) {
    AUTO("auto"),
    ON("on"),
    OFF("off");

    companion object {
        fun of(id: String?): TouchMode = values().firstOrNull { it.id == id } ?: AUTO
    }
}

/**
 * The connected input devices. The launcher lists them and hands them to the
 * game, where any of them takes over from the on-screen pad: the game can see
 * controllers and wheels itself, but only Android can list keyboards and mice.
 *
 * A touchscreen is not a device here. It reports itself as a mouse as well, and
 * the finger is exactly what the on-screen pad is for.
 */
object InputDevices {

    fun list(context: Context): List<ConnectedDevice> {
        val manager = context.getSystemService(Context.INPUT_SERVICE) as? InputManager
            ?: return emptyList()
        val found = mutableListOf<ConnectedDevice>()
        for (id in manager.inputDeviceIds) {
            val source = manager.getInputDevice(id) ?: continue
            kindOf(source)?.let { found += ConnectedDevice(id, source.name ?: "?", it) }
        }
        return found
    }

    /** The platform flags for a list of devices. */
    fun flags(devices: List<ConnectedDevice>): Int = devices.fold(0) { all, device -> all or device.kind }

    /** Hot plug: the system calls back on add, remove and change. */
    fun listener(onChanged: () -> Unit): InputManager.InputDeviceListener =
        object : InputManager.InputDeviceListener {
            override fun onInputDeviceAdded(deviceId: Int) = onChanged()
            override fun onInputDeviceRemoved(deviceId: Int) = onChanged()
            override fun onInputDeviceChanged(deviceId: Int) = onChanged()
        }

    /** A gamepad or wheel, a keyboard or a mouse; null for anything else. */
    private fun kindOf(device: InputDevice): Int? {
        val sources = device.sources
        if (sources and InputDevice.SOURCE_TOUCHSCREEN != 0) return null
        if (sources and InputDevice.SOURCE_GAMEPAD != 0) return DeviceKind.GAMEPAD
        if (sources and InputDevice.SOURCE_JOYSTICK != 0) return DeviceKind.GAMEPAD   /* wheels report this one */
        if (sources and InputDevice.SOURCE_KEYBOARD != 0) return DeviceKind.KEYBOARD
        if (sources and InputDevice.SOURCE_MOUSE != 0) return DeviceKind.MOUSE
        return null
    }
}
