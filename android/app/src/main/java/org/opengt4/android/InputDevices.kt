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
class ConnectedDevice(
    val id: Int,
    val name: String,
    val kind: Int,
    val sources: Int,
    val vendorId: Int,
    val productId: Int,
    val axes: String,
)

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
            kindOf(source)?.let {
                found += ConnectedDevice(id, source.name ?: "?", it, source.sources,
                    source.vendorId, source.productId, axesOf(source))
            }
        }
        return found
    }

    /** The platform flags for a list of devices. */
    fun flags(devices: List<ConnectedDevice>): Int = devices.fold(0) { all, device -> all or device.kind }

    /** The kinds it recognised, spelled out for the log. */
    fun describe(devices: List<ConnectedDevice>): String =
        if (devices.isEmpty()) "none"
        else devices.joinToString("; ") {
            "${it.name}[id=${it.id} ${kindName(it.kind)} sources=0x${Integer.toHexString(it.sources)}" +
                " vendor=0x${Integer.toHexString(it.vendorId)} product=0x${Integer.toHexString(it.productId)}" +
                " axes=(${it.axes})]"
        }

    /**
     * Every device the system reports, recognised or not. A pad that never
     * appears on the screen has to be readable somewhere, or the reason is a
     * guess: this is the raw list, with the two fields the policy turns on.
     */
    fun dump(context: Context): String {
        val manager = context.getSystemService(Context.INPUT_SERVICE) as? InputManager
            ?: return "no input service"
        val lines = mutableListOf<String>()
        for (id in manager.inputDeviceIds) {
            val device = manager.getInputDevice(id) ?: continue
            lines += "id=$id name=\"${device.name}\" sources=0x${Integer.toHexString(device.sources)}" +
                " keyboardType=${device.keyboardType}" +
                " vendor=0x${Integer.toHexString(device.vendorId)} product=0x${Integer.toHexString(device.productId)}" +
                " axes=(${axesOf(device)}) kind=${kindOf(device)?.let { kindName(it) } ?: "ignored"}"
        }
        return if (lines.isEmpty()) "none" else lines.joinToString("\n")
    }

    private fun kindName(kind: Int): String = when (kind) {
        DeviceKind.GAMEPAD -> "gamepad"
        DeviceKind.KEYBOARD -> "keyboard"
        else -> "mouse"
    }

    private fun axesOf(device: InputDevice): String =
        device.motionRanges.joinToString(",") { it.axis.toString() }

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
        /* A finger or a pen is what the on-screen pad is for, not a reason to
           hide it: a stylus is also reported as a mouse. */
        if (sources and InputDevice.SOURCE_TOUCHSCREEN != 0) return null
        if (sources and InputDevice.SOURCE_STYLUS != 0) return null
        /* A joystick with no motion range is a name, not a pad. Some keyboards
           carry a stray gamepad bit, and hiding the controls for one of those
           leaves a tablet with no way to drive. */
        val pad = sources and (InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK)
        if (pad != 0 && device.motionRanges.isNotEmpty()) return DeviceKind.GAMEPAD
        if (sources and InputDevice.SOURCE_KEYBOARD != 0 &&
            device.keyboardType != InputDevice.KEYBOARD_TYPE_NONE) return DeviceKind.KEYBOARD
        if (sources and InputDevice.SOURCE_MOUSE != 0) return DeviceKind.MOUSE
        return null
    }
}
