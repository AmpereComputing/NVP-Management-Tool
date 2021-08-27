# Ampere Computing NVP Management Tool

The Ampere Computing NVP Management Tool (nvparm) is an engineering tool
that runs on the BMC console and enables users to edit Dynamic NVPARAM
fields within the AmpereOne Host SPI-NOR and Boot Strap Data (BSD) EEPROM devices.

The Ampere Computing NVP Management Tool currently supports the follow Ampere Computing products:
- AmpereOne

Users can use nvparm tool to:

- Read an NVPARAM field individually and its associated valid bit
  from an NVP file of Dynamic or Static NVPARAM or BSD partitions.

- Write NVPARAM data to a field individually in a NVP file of Dynamic or
  Static NVPARAM or BSD partitions.

- Enable or disable the valid bit of an NVPARAM field in an NPV file.

- Erase an NVPARAM field in an NVP file.

- Dumping an NVP file into a binary file on BMC’s file system.

- Write new content to an existing NVP file.

- Printing GPT header.

## Unsupported operations

- Flashing a new *.nvp file to a partition.

## Usage

Read a field and its associated valid bit at field_index of nvp_file at
nvp_part partition:

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -i <field_index> -r
```

Enable or disable valid bit at field_index in nvp_file at nvp_part partition.

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -i <field_index> -v <valid_bit>
```

Write data to a field and its associated valid bit at field_index in nvp_file
at nvp_part partition.

```text
# nvparm -t [-D <device>] <nvp_part> -f <nvp_file> -i <field_index> -v <valid_bit> -w <nvp_data>
```

Write data to a field at field_index in nvp_file at nvp_part partition.
The valid bit is enabled by default.

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -i <field_index> -w <nvp_data>
```

Erase a field at field_index of nvp_file at nvp_part partition.

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -i <field_index> -e
```

Dump specific NVP file into raw file.

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -d <raw_file>
```

Write content of new_nvp_file to nvp_file.

```text
# nvparm [-D <device>] -t <nvp_part> -f <nvp_file> -o <new_nvp_file>
```

Read an EEPROM field and its associated valid bit.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -r
```

Enable or disable the valid bit of an EEPROM field_index.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -v <valid_bit>
```

Write data to an EEPROM field and its associated valid bit at field_index.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -v <valid_bit> -w <nvp_data>
```

Write data to an EEPROM field at field_index. The valid bit is enabled by default.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -w <nvp_data>
```

Erase an EEPROM field at field_index.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -e
```

Dump the Boot Strap Data NVP EEPROM file into raw file

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -d <raw_file>
```

Write content of new_nvp_file to nvp_file.

```text
# nvparm -t nvparamb -b <i2c_bus> -s <target_addr> -f <nvp_file> -o <new_nvp_file>
```

Print GPT header. NVP partition names and GUIDs are displayed with this option.

```text
# nvparm [-D <device>] -p
```

Print help message.

```text
# nvparm -h
```

### Note

- *-D* is optional. Users use it to specify the MTD partition (e.g. /dev/mtd12).
By default, the tool works on *hnor* MTD partition label.

- Use partition GUIDs

  Besides using partition names, users can use partition GUIDs to access
a NVPARAM partition. Commands with partition GUIDs as following:

  - Read a field and its associated valid bit at field_index of nvp_file at
the NVP partition with GUID nvp_guid.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -i <field_index> -r
    ```

  - Enable or disable valid bit at field_index in nvp_file at the NVP partition
with GUID nvp_guid.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -i <field_index> -v <valid_bit>
    ```

  - Write data to a field and its associated valid bit at field_index of nvp_file
at the NVP partition with GUID nvp_guid.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -i <field_index> -v <valid_bit> -w <nvp_data>
    ```

  - Write data to a field at field_index in nvp_file at the NVP partition with GUID
nvp_guid. The valid bit is enabled by default.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -i <field_index> -w <nvp_data>
    ```

  - Erase field at field_index of nvp_file at the NVP partition with GUID nvp_guid.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -i <field_index> -e
    ```

  - Dump specific NVP file into raw file at the NVP partition with GUID nvp_guid.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -d <raw_file>
    ```

  - Write content of new_nvp_file to nvp_file.

    ```text
    # nvparm [-D <device>] -u <nvp_guid> -f <nvp_file> -o <new_nvp_file>
    ```

  - Read an EEPROM field and its associated valid bit.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -r
    ```

  - Enable or disable the valid bit of an EEPROM field_index.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -v <valid_bit>
    ```

  - Write data to an EEPROM field and its associated valid bit at field_index.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -v <valid_bit> -w <nvp_data>
    ```

  - Write data to an EEPROM field at field_index. The valid bit is enabled by default.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -w <nvp_data>
    ```

  - Erase an EEPROM field at field_index.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -i <field_index> -e
    ```

  - Dump the Boot Strap Data NVP EEPROM file into raw file

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -d <raw_file>
    ```

  - Write content of new_nvp_file to nvp_file.

    ```text
    # nvparm -u 0 -b <i2c_bus> -s <target_addr> -f <nvp_file> -o <new_nvp_file>
    ```

## Arguments

- nvp_part: Partition name of Dynamic NVPARAM or Static NVPARAM.
  Users can get partition names with option -p. Specially,
  nvparamb is the fixed partition name for Boot Strap Data partition.

- nvp_guid: Partition’s GUID from the GPT header. Users can use either nvp_guid
  or nvp_part option but not both at a time. Specially, 0 is the fixed partition
  GUID for Boot Strap Data partition.

- nvp_file: Name of NVP file. Specially, NVPBERLY is the fixed nvp file for
  Boot Strap Data partition.

- new_nvp_file: Name of new NVP file (include path).

- field_index: Index of the target field in nvp file, start from 0.

- nvp_data: The data to write into the field (as Hexadecimal).

- valid_bit: The valid bit associated to the field_index.
  Can be set with the following values:

  - 0x0: the field is disabled.

  - 0x1: the field is enabled.

- i2c_bus: The I2C bus number. Default is 10 (I2C11)

- target_addr: The target address of the EEPROM. Default is 0x50

- device: Specify the MTD partition e.g. /dev/mtd12

## Notes

Updating NVPS (Static) values may conflict with Platform Secure Boot signing.
The Static NVP partitions (NVPS) are signed as part of platform secure boot,
and modifying them at runtime may prevent the system from rebooting correctly.

Users must do the following steps before using nvparm tool on Mt.Mitchell.
Set the GPIOs correctly to access the right SPI-NOR/EEPROM via the MUX.

Select the MUX:

```text
# gpiotool --set-data-high 182
```

For primary device (only if access to SPI-NOR):

```text
# gpiotool --set-data-high 183
```

For secondary device (only if access to SPI-NOR):

```text
# gpiotool --set-data-low 183
```

Bind the device (only if access to SPI-NOR):

- There are 2 ways to bind (belong to the Kernel driver) as following:
  - Use ASPEED SMC driver:

    ```text
    # echo 1e630000.spi > /sys/bus/platform/drivers/aspeed-smc/bind
    ```

  - Directly use Kernel SPI-NOR driver:

    ```text
    # echo spi1.0 > /sys/bus/spi/drivers/spi-nor/bind
    ```

Users must execute the following commands after using nvparm tool.
Unbind the SPI-NOR device:

- There are 2 ways to unbind (belong to the Kernel driver) as following:
  - Use ASPEED SMC driver:

    ```text
    # echo 1e630000.spi > /sys/bus/platform/drivers/aspeed-smc/unbind
    ```

  - Directly use Kernel SPI-NOR driver:

    ```text
    # echo spi1.0 > /sys/bus/spi/drivers/spi-nor/bind
    ```

Reset GPIO settings to default values.

```text
# gpiotool --set-data-high 183 // If access to SPI-NOR
# gpiotool --set-data-low 182
```

## Examples

1. Using nvparm to read an NVPARAM

- Read NVPARAM field at index 0 of nvpdddr0.nvp file at Dynamic NVP partition:

  - If read a 8-bytes NVPARAM field

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -r
    0x01 0xffffffffffffffff

    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -r
    0x01 0xffffffffffffffff
    ```

  - If read a 4-bytes NVPARAM field

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -r
    0x01 0xffffffff

    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -r
    0x01 0xffffffff
    ```

    **Note**: The first number (0x01) is the Valid bit (associates with target
    field) and the second number is the Field data (Hexadecimal)

2. Using nvparm to write an NVPARAM entry in a file.

- Write field data and its valid bit at index 0 of nvpdddr0.nvp file at
  Dynamic NVP partition

  - If write to a 8-bytes NVPARAM field and enable the valid bit.

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff -v 0x01
    or:
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff -v 0x01
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff
    ```

  - If write to a 8-bytes NVPARAM field and disable the valid bit.

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff -v 0x00
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -w 0xffffffffffffffff -v 0x00
    ```

  - If write to a 4-bytes NVPARAM field and enable the valid bit.

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -w 0xffffffff

    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -w 0xffffffff
    ```

- Write the valid bit at index 0 of nvpdddr0.nvp file at Dynamic NVP partition

  - Enable the valid bit.

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -v 0x01
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -v 0x01
    ```

  - Disable the valid bit.

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -v 0x00
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -v 0x00
    ```

3. Using nvparm to erase an NVPARAM

- Erase field at index 0 of nvpdddr0.nvp file at Dynamic NVP partition

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -e
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -e
    ```

4. Using nvparm to dump a specify NVP file into raw file

    ```text
    # cd /var/
    # nvparm -t nvpd -f nvpdddr0.nvp -d raw.bin
    or:
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -d raw.bin
    ```

5. Print GPT header

    ```text
    # nvparm -p
    ---------------------------------------
    [GPT Partition #0]
    Name: certs
    GUID: 00000000-0000-0000-0000-000000000000
    ---------------------------------------
    [GPT Partition #1]
    Name: secpro
    GUID: 00000000-0000-0000-0000-000000000000
    ---------------------------------------
    [GPT Partition #2]
    Name: mpro
    GUID: 52121B34-F0E8-4E27-B2D1-C02AA79FCE9D
    ---------------------------------------
    [GPT Partition #3]
    Name: atf
    GUID: 89355511-A5AE-44E3-88BA-1E4CC34DDD02
    ---------------------------------------
    ```

6. Use option -D to specify MTD partition:

    ```text
    # nvparm -D /dev/mtd12 -t nvpd -f nvpdddr0.nvp -i 4 -r
    0x01 0x00000013
    ```

7. Common error cases.

- The SPI NOR is corrupted or not a valid GPT disk:

    ```text
    # nvparm -t nvpd -f nvpdddr0.nvp -i 0 -e
    The GPT flash disk is corrupted.
    EXIT!
    ```

    or:

    ```text
    # nvparm -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -e
    The GPT flash disk is corrupted.
    EXIT!
    ```

- Invalid mixing of options, e.g. -t and -u

    ```text
    # nvparm -t nvpd -u 44E342FA-21F6-4299-B712-71F083BDE48C -f nvpdddr0.nvp -i 0 -e
    Mixing -t and -u options is not allowed!
    EXIT!
    ```
