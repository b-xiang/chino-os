//
// Kernel System
//
#include <libbsp/bsp.hpp>
#include <kernel/device/DeviceManager.hpp>
#include <kernel/threading/ProcessManager.hpp>
#include <kernel/diagnostic/KernelLogger.hpp>
#include <kernel/memory/MemoryManager.hpp>
#include <kernel/object/ObjectManager.hpp>
#include <kernel/threading/ThreadSynchronizer.hpp>
#include <kernel/device/io/Gpio.hpp>
#include <kernel/device/io/I2c.hpp>
#include <kernel/device/storage/Storage.hpp>

using namespace Chino;
using namespace Chino::Device;
using namespace Chino::Threading;

void Chino::BSPSystemStartup()
{
	auto access = OA_Read | OA_Write;
	auto gpio = g_ObjectMgr->GetDirectory(WKD_Device).Open("gpio1", access).MoveAs<GpioController>();
	auto pin0 = gpio->OpenPin(0, access);
	pin0->SetDriveMode(GpioPinDriveMode::Output);

	auto eeprom1 = g_ObjectMgr->GetDirectory(WKD_Device).Open("eeprom1", access).MoveAs<EEPROMStorage>();
	uint8_t buffer[10] = { 2 };
	{
		gsl::span<const uint8_t> writeBuffers[] = { { buffer, 1 } };
		eeprom1->Write(0, { writeBuffers });
	}
	{
		gsl::span<uint8_t> readBuffers[] = { { buffer, 1 } };
		eeprom1->Read(0, { readBuffers });
		g_Logger->PutFormat("AT24C02 Read: %d\n", buffer[0]);
	}

	auto proc = g_ProcessMgr->GetCurrentThread()->GetProcess();
	auto semp = MakeObject<Semaphore>(0);
	auto mutex = MakeObject<Mutex>();
	proc->AddThread([&]
	{
		while (true)
		{
			Locker<Mutex> locker(mutex);
			pin0->Write(GpioPinValue::Low);

			semp->Take(1);

			pin0->Write(GpioPinValue::High);

			semp->Take(1);
		}
	}, 1);

	proc->AddThread([&]
	{
		while (true)
		{
			for (size_t i = 0; i < 100; i++)
				ArchHaltProcessor();
			semp->Give(1);
		}
	}, 1);

	while (1)
		ArchHaltProcessor();
}
