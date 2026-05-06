Vocabulary:

| Term    | Desc                                               | Context      |
| ------- | -------------------------------------------------- | ------------ |
| HF-AG   | Hands Free Audio Gateway                           |              |
| BTC     | Bluetooth Classic                                  |              |
| BTC     | Bluetooth Transport Controller                     |              |
| BTU/BTA | Bluetooth Upper layer                              | Bluedroid    |
| JV      | Java                                               | Android APIs |
| GAP     | Generic Access Profile                             |              |
| GATT    | Generic ATTribute Profile                          |              |
| A2DP    | Advanced Audio Distribution Profile                |              |
| L2CAP   | Logical Link Control and Adaptation Layer Protocol |              |
| SDP     | Service Discovery Protocol                         |              |
| SPP     | Serial Port Protocol                               |              |

Bluetooth classic:
- Service discovery protocol (SDP)
- Communicate on RFCOMM channels
- Headsets, health devices, phones
Bluetooth Low Energy:
- Advertisements
- Generic Attribute Table: GATT

GATT (Generic Attribute Table):
- Uses a hierarchy of profiles/services/characteristics to organize and standardize protocols and features, and allow vendor extensions
- **Profiles**
	- Define client/server behavior, security handling, required services, etc
	- **Services**
		- Used to group characteristics together
		- **Characteristics**
			- Properties
			- Value
			- **Descriptor**
				- Has info on the value type
