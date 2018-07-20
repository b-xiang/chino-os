﻿//
// Kernel Device
//
#include "ili9486.hpp"
#include <libdriver/devicetree/intel/display/lcd/lcd8080.hpp>
#include <kernel/kdebug.hpp>
#include <kernel/object/ObjectManager.hpp>
#include <kernel/device/DeviceManager.hpp>
#include <libbsp/bsp.hpp>
#include <string>
#include <array>

using namespace Chino;
using namespace Chino::Device;
using namespace Chino::Threading;

DEFINE_FDT_DRIVER_DESC_1(ILI9486Driver, "display", "ilitek,ili9486");

#define WHITE          0xFFFF
#define BLACK          0x0000
#define BLUE           0x001F
#define RED            0xF800
#define MAGENTA        0xF81F
#define GREEN          0x07E0
#define CYAN           0x7FFF
#define YELLOW         0xFFE0

enum Command : uint16_t
{
	Cmd_SleepOut = 0x11,
	Cmd_DisplayOn = 0x29,
	Cmd_ColumnAddressSet = 0x2A,
	Cmd_PageAddressSet = 0x2B,
	Cmd_MemoryWrite = 0x2C,
	Cmd_InterfacePixelFormat = 0x3A,
	Cmd_InterfaceModeControl = 0xB0,
	Cmd_ReadID4 = 0xD3
};

enum PixeFormat
{
	PixelFormat_16bit = 0b0101,
	PixelFormat_18bit = 0b0110
};

class ILI9486Device : public Device, public ExclusiveObjectAccess
{
public:
	static constexpr uint16_t PixelWidth = 320;
	static constexpr uint16_t PixelHeight = 240;

	ILI9486Device(const FDTDevice& fdt)
		:fdt_(fdt)
	{
		g_ObjectMgr->GetDirectory(WKD_Device).AddItem(fdt.GetName(), *this);
	}

	virtual void OnFirstOpen() override
	{
		auto access = OA_Read | OA_Write;
		lcd_ = g_ObjectMgr->GetDirectory(WKD_Device).Open(fdt_.GetProperty("driver")->GetString(), access).MoveAs<LCD8080Controller>();
		Initialize();
	}

	virtual void OnLastClose() override
	{
		lcd_.Reset();
	}

private:
	void Write(uint16_t reg, gsl::span<const uint16_t> data)
	{
		gsl::span<const uint16_t> buffers[] = { data };
		lcd_->WriteRead(reg, { buffers }, {});
	}

	void Read(uint16_t reg, gsl::span<uint16_t> data)
	{
		gsl::span<uint16_t> buffers[] = { data };
		lcd_->WriteRead(reg, {}, { buffers });
	}

	void SendCmd(uint16_t reg)
	{
		lcd_->WriteRead(reg, {}, {});
	}

	void Initialize()
	{
		SetInterfaceMode(0);
		SleepOut();
		SetInterfacePixelFormat(PixelFormat_16bit, PixelFormat_16bit);
		DisplayOn();
		ClearScreen(GREEN);
	}

	uint32_t ReadID4()
	{
		uint16_t id[4];
		Read(Cmd_ReadID4, id);
		return (id[2] << 8) | id[3];
	}

	void SetInterfaceMode(uint8_t value)
	{
		uint16_t data[] = { value };
		Write(Cmd_InterfaceModeControl, data);
	}

	void SleepOut()
	{
		SendCmd(Cmd_SleepOut);
		BSPSleepMs(5);
	}

	void SetInterfacePixelFormat(PixeFormat cpu, PixeFormat rgb)
	{
		uint16_t data[] = { uint16_t(cpu | (rgb << 4)) };
		Write(Cmd_InterfacePixelFormat, data);
	}

	void DisplayOn()
	{
		SendCmd(Cmd_DisplayOn);
		BSPSleepMs(5);
	}

	void SetAccessRegion(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom)
	{
		{
			uint16_t data[] = { uint16_t(left >> 8), uint16_t(left & 0xFF), uint16_t(right >> 8), uint16_t(right & 0xFF) };
			Write(Cmd_ColumnAddressSet, data);
		}
		{
			uint16_t data[] = { uint16_t(top >> 8), uint16_t(top & 0xFF), uint16_t(bottom >> 8), uint16_t(bottom & 0xFF) };
			Write(Cmd_PageAddressSet, data);
		}
	}

	void SetFullAccessRegion()
	{
		SetAccessRegion(0, PixelWidth - 1, 0, PixelHeight - 1);
	}

	void ClearScreen(uint16_t color)
	{
		SetFullAccessRegion();
		lcd_->Fill(Cmd_MemoryWrite, color, PixelWidth * PixelHeight);
	}
private:
	const FDTDevice& fdt_;
	ObjectAccessor<LCD8080Controller> lcd_;
};

ILI9486Driver::ILI9486Driver(const FDTDevice& device)
	:device_(device)
{

}

void ILI9486Driver::Install()
{
	g_DeviceMgr->InstallDevice(MakeObject<ILI9486Device>(device_));
}