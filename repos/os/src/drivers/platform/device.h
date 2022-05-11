/*
 * \brief  Platform driver - Device abstraction
 * \author Stefan Kalkowski
 * \date   2020-04-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__DEVICE_H_
#define _SRC__DRIVERS__PLATFORM__DEVICE_H_

#include <base/allocator.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <pci/types.h>
#include <platform_session/device.h>
#include <util/list.h>
#include <util/list_model.h>
#include <util/reconstructible.h>
#include <util/xml_generator.h>

#include <clock.h>
#include <reset.h>
#include <power.h>

namespace Driver {

	using namespace Genode;

	class  Device;
	struct Device_model;
	class  Session_component;
	struct Irq_update_policy;
	struct Io_mem_update_policy;
	struct Io_port_update_policy;
	struct Property_update_policy;
	struct Clock_update_policy;
	struct Reset_domain_update_policy;
	struct Power_domain_update_policy;
	struct Pci_config_update_policy;
}


class Driver::Device : private List_model<Device>::Element
{
	public:

		using Name  = Genode::String<64>;
		using Type  = Genode::String<64>;
		using Range = Platform::Device_interface::Range;

		struct Owner
		{
			void * obj_id;

			Owner() : obj_id(nullptr) {}
			Owner(Session_component & session);

			bool operator == (Owner const & o) const {
				return obj_id == o.obj_id; }

			bool valid() const {
				return obj_id != nullptr; }
		};

		struct Io_mem : List_model<Io_mem>::Element
		{
			Range range;

			Io_mem(Range range) : range(range) {}
		};

		struct Irq : List_model<Irq>::Element
		{
			enum Type { LEGACY, MSI, MSIX };

			unsigned              number;
			Type                  type     { LEGACY                          };
			Irq_session::Polarity polarity { Irq_session::POLARITY_UNCHANGED };
			Irq_session::Trigger  mode     { Irq_session::TRIGGER_UNCHANGED  };

			Irq(unsigned number) : number(number) {}
		};

		struct Io_port_range : List_model<Io_port_range>::Element
		{
			uint16_t addr;
			uint16_t size;

			Io_port_range(uint16_t addr, uint16_t size)
			: addr(addr), size(size) {}
		};

		struct Property : List_model<Property>::Element
		{
			using Name  = Genode::String<64>;
			using Value = Genode::String<64>;

			Name  name;
			Value value;

			Property(Name name, Value value)
			: name(name), value(value) {}
		};

		struct Clock : List_model<Clock>::Element
		{
			using Name  = Genode::String<64>;

			Name          name;
			Name          parent;
			Name          driver_name;
			unsigned long rate;

			Clock(Name name,
			      Name parent,
			      Name driver_name,
			      unsigned long rate)
			: name(name), parent(parent),
			  driver_name(driver_name), rate(rate) {}
		};

		struct Power_domain : List_model<Power_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Power_domain(Name name) : name(name) {}
		};

		struct Reset_domain : List_model<Reset_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Reset_domain(Name name) : name(name) {}
		};

		struct Pci_config : List_model<Pci_config>::Element
		{
			addr_t        addr;
			Pci::bus_t    bus_num;
			Pci::dev_t    dev_num;
			Pci::func_t   func_num;
			Pci::vendor_t vendor_id;
			Pci::device_t device_id;
			Pci::class_t  class_code;
			bool          bridge;

			Pci_config(addr_t        addr,
			           Pci::bus_t    bus_num,
			           Pci::dev_t    dev_num,
			           Pci::func_t   func_num,
			           Pci::vendor_t vendor_id,
			           Pci::device_t device_id,
			           Pci::class_t  class_code,
			           bool          bridge)
			:
				addr(addr),
				bus_num(bus_num),
				dev_num(dev_num),
				func_num(func_num),
				vendor_id(vendor_id),
				device_id(device_id),
				class_code(class_code),
				bridge(bridge) {}
		};

		Device(Name name, Type type);
		virtual ~Device();

		Name  name() const;
		Type  type() const;
		Owner owner() const;

		virtual void acquire(Session_component &);
		virtual void release(Session_component &);

		template <typename FN> void for_each_irq(FN const & fn)
		{
			unsigned idx = 0;
			_irq_list.for_each([&] (Irq const & irq) {
				fn(idx++, irq.number, irq.type, irq.polarity, irq.mode); });
		}

		template <typename FN> void for_each_io_mem(FN const & fn)
		{
			unsigned idx = 0;
			_io_mem_list.for_each([&] (Io_mem const & iomem) {
				fn(idx++, iomem.range); });
		}

		template <typename FN> void for_each_io_port_range(FN const & fn)
		{
			unsigned idx = 0;
			_io_port_range_list.for_each([&] (Io_port_range const & ipr) {
				fn(idx++, ipr.addr, ipr.size); });
		}

		template <typename FN> void for_pci_config(FN const & fn)
		{
			/*
			 * we allow only one PCI config per device,
			 * even if more were declared
			 */
			bool found = false;
			_pci_config_list.for_each([&] (Pci_config const & cfg) {
				if (found) {
					warning("Only one pci-config is supported per device!");
					return;
				}
				found = true;
				fn(cfg);
			});
		}

		void report(Xml_generator &, Device_model &, bool);

	protected:

		virtual void _report_platform_specifics(Xml_generator &,
		                                        Device_model  &) {}

		friend class Driver::Device_model;
		friend class List_model<Device>;
		friend class List<Device>;

		Name                      _name;
		Type                      _type;
		Owner                     _owner {};
		List_model<Io_mem>        _io_mem_list {};
		List_model<Irq>           _irq_list {};
		List_model<Io_port_range> _io_port_range_list {};
		List_model<Property>      _property_list {};
		List_model<Clock>         _clock_list {};
		List_model<Power_domain>  _power_domain_list {};
		List_model<Reset_domain>  _reset_domain_list {};
		List_model<Pci_config>    _pci_config_list {};

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);
};


class Driver::Device_model :
	public List_model<Device>::Update_policy
{
	private:

		Heap               & _heap;
		List_model<Device>   _model  { };
		Clocks               _clocks { };
		Resets               _resets { };
		Powers               _powers { };

		Constructible<Expanding_reporter> & _reporter;

	public:

		void update_report();
		void update(Xml_node const & node);

		Device_model(Heap & heap,
		             Constructible<Expanding_reporter> & reporter)
		: _heap(heap), _reporter(reporter) { }

		~Device_model() {
			_model.destroy_all_elements(*this); }

		template <typename FN>
		void for_each(FN const & fn) { _model.for_each(fn); }


		/***********************
		 ** Update_policy API **
		 ***********************/

		void        destroy_element(Device & device);
		Device &    create_element(Xml_node node);
		void        update_element(Device & device, Xml_node node);
		static bool element_matches_xml_node(Device const & dev,
		                                     Genode::Xml_node n)
		{
			return dev.name() == n.attribute_value("name", Device::Name()) &&
			       dev.type() == n.attribute_value("type", Device::Type());
		}

		static bool node_is_element(Genode::Xml_node node) {
			return node.has_type("device"); }

		Clocks & clocks() { return _clocks; };
		Resets & resets() { return _resets; };
		Powers & powers() { return _powers; };
};


struct Driver::Irq_update_policy : Genode::List_model<Device::Irq>::Update_policy
{
	Genode::Allocator & alloc;

	Irq_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & irq) {
		Genode::destroy(alloc, &irq); }

	Element & create_element(Genode::Xml_node node)
	{
		unsigned  number = node.attribute_value<unsigned>("number", 0);
		Element & elem   = *(new (alloc) Element(number));

		String<16> polarity = node.attribute_value("polarity", String<16>());
		String<16> mode     = node.attribute_value("mode",     String<16>());
		String<16> type     = node.attribute_value("type",     String<16>());
		if (polarity.valid())
			elem.polarity = (polarity == "high") ? Irq_session::POLARITY_HIGH
			                                     : Irq_session::POLARITY_LOW;
		if (mode.valid())
			elem.mode = (mode == "edge") ? Irq_session::TRIGGER_EDGE
			                             : Irq_session::TRIGGER_LEVEL;
		if (type.valid())
			elem.type = (type == "msi-x") ? Element::MSIX : Element::MSI;

		return elem;
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & irq, Genode::Xml_node node)
	{
		unsigned number = node.attribute_value<unsigned>("number", 0);
		return number == irq.number;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("irq");
	}
};


struct Driver::Io_mem_update_policy : Genode::List_model<Device::Io_mem>::Update_policy
{
	Genode::Allocator & alloc;

	Io_mem_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & iomem) {
		Genode::destroy(alloc, &iomem); }

	Element & create_element(Genode::Xml_node node)
	{
		Device::Range range { node.attribute_value<Genode::addr_t>("address", 0),
		                      node.attribute_value<Genode::size_t>("size",    0) };
		return *(new (alloc) Element(range));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & iomem, Genode::Xml_node node)
	{
		Device::Range range { node.attribute_value<Genode::addr_t>("address", 0),
		                      node.attribute_value<Genode::size_t>("size",    0) };
		return (range.start == iomem.range.start) && (range.size == iomem.range.size);
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("io_mem");
	}
};


struct Driver::Io_port_update_policy
: Genode::List_model<Device::Io_port_range>::Update_policy
{
	Genode::Allocator & alloc;

	Io_port_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & ipr) {
		Genode::destroy(alloc, &ipr); }

	Element & create_element(Genode::Xml_node node)
	{
		uint16_t addr = node.attribute_value<uint16_t>("address", 0);
		uint16_t size = node.attribute_value<uint16_t>("size",    0);
		return *(new (alloc) Element(addr, size));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & ipr, Genode::Xml_node node)
	{
		uint16_t addr = node.attribute_value<uint16_t>("address", 0);
		uint16_t size = node.attribute_value<uint16_t>("size",    0);
		return addr == ipr.addr && size == ipr.size;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("io_port_range");
	}
};


struct Driver::Property_update_policy : Genode::List_model<Device::Property>::Update_policy
{
	Genode::Allocator & alloc;

	Property_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & p) {
		Genode::destroy(alloc, &p); }

	Element & create_element(Genode::Xml_node node)
	{
		return *(new (alloc)
			Element(node.attribute_value("name",  Element::Name()),
			        node.attribute_value("value", Element::Value())));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & prop, Genode::Xml_node node)
	{
		Element::Name  n = node.attribute_value("name", Element::Name());
		Element::Value v = node.attribute_value("value", Element::Value());
		return (n == prop.name) && (v == prop.value);
	}

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("property"); }
};


struct Driver::Clock_update_policy
: Genode::List_model<Device::Clock>::Update_policy
{
	Genode::Allocator & alloc;

	Clock_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & clock) {
		Genode::destroy(alloc, &clock); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name   = node.attribute_value("name",        Element::Name());
		Element::Name parent = node.attribute_value("parent",      Element::Name());
		Element::Name driver = node.attribute_value("driver_name", Element::Name());
		unsigned long rate   = node.attribute_value<unsigned long >("rate", 0);
		return *(new (alloc) Element(name, parent, driver, rate));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & clock, Genode::Xml_node node)
	{
		Element::Name name        = node.attribute_value("name", Element::Name());
		Element::Name driver_name = node.attribute_value("driver_name", Element::Name());
		return name == clock.name && driver_name == clock.driver_name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("clock");
	}
};


struct Driver::Power_domain_update_policy
: Genode::List_model<Device::Power_domain>::Update_policy
{
	Genode::Allocator & alloc;

	Power_domain_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & pd) {
		Genode::destroy(alloc, &pd); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return *(new (alloc) Element(name));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & pd, Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return name == pd.name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("power-domain");
	}
};


struct Driver::Reset_domain_update_policy
: Genode::List_model<Device::Reset_domain>::Update_policy
{
	Genode::Allocator & alloc;

	Reset_domain_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & pd) {
		Genode::destroy(alloc, &pd); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return *(new (alloc) Element(name));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & pd, Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return name == pd.name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("reset-domain");
	}
};


struct Driver::Pci_config_update_policy
: Genode::List_model<Device::Pci_config>::Update_policy
{
	Genode::Allocator & alloc;

	Pci_config_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & pd) {
		Genode::destroy(alloc, &pd); }

	Element & create_element(Genode::Xml_node node)
	{
		using namespace Pci;

		addr_t   addr       = node.attribute_value("address", ~0UL);
		bus_t    bus_num    = node.attribute_value<bus_t>("bus", 0);
		dev_t    dev_num    = node.attribute_value<dev_t>("device", 0);
		func_t   func_num   = node.attribute_value<func_t>("function", 0);
		vendor_t vendor_id  = node.attribute_value<vendor_t>("vendor_id",
		                                                     0xffff);
		device_t device_id  = node.attribute_value<device_t>("device_id",
		                                                     0xffff);
		class_t  class_code = node.attribute_value<class_t>("class", 0xff);
		bool     bridge     = node.attribute_value("bridge", false);

		return *(new (alloc) Element(addr, bus_num, dev_num, func_num,
		                             vendor_id, device_id, class_code, bridge));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & e, Genode::Xml_node node)
	{
		addr_t addr = node.attribute_value("address", ~0UL);
		return addr == e.addr;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("pci-config");
	}
};

#endif /* _SRC__DRIVERS__PLATFORM__DEVICE_H_ */
