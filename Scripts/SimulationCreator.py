#!/usr/bin/env python3

import os
import subprocess
import shutil
import xml.etree.ElementTree as ET

# ================= PATHS =================

HOME = os.path.expanduser("~")

BASE = f"{HOME}/contiki-ng"
COOJA = f"{BASE}/tools/cooja"
SIM_DIR = f"{BASE}/Simulations"
LOG_DIR = f"{HOME}/Documents/Logs"

COOJA_LOG = f"{COOJA}/COOJA.testlog"

# ================= CONFIG =================

RSU_COUNT = 6
SEED = 123456

FW = {
    "BGKP": {
        "path": "examples/BGKP",
        "rsu": "rsu.c",
        "mbl": "mbl.c",
        "rsu_bin": "rsu.sky",
        "mbl_bin": "mbl.sky"
    },
    "DUMB": {
        "path": "examples/BGKP/DUMB",
        "rsu": "dumb_rsu.c",
        "mbl": "dumb_mbl.c",
        "rsu_bin": "dumb_rsu.sky",
        "mbl_bin": "dumb_mbl.sky"
    },
    "FLUD": {
        "path": "examples/BGKP/UDP",
        "rsu": "udp-rsu.c",
        "mbl": "udp-mobile.c",
        "rsu_bin": "udp-rsu.sky",
        "mbl_bin": "udp-mobile.sky"
    }
}

# ================= HELPERS =================

def run(cmd, cwd=None):
    subprocess.run(cmd, shell=True, cwd=cwd, check=True)


def indent(elem, level=0):
    """
    Pretty-print XML (adds indentation and newlines)
    """
    i = "\n" + level * "  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        for e in elem:
            indent(e, level + 1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


# ================= MOTE =================

def create_mote(parent, mote_id):
    mote = ET.SubElement(parent, "mote")

    pos = ET.SubElement(mote, "interface_config")
    pos.text = "org.contikios.cooja.interfaces.Position"
    ET.SubElement(pos, "pos", x="0.0", y="0.0")

    mid = ET.SubElement(mote, "interface_config")
    mid.text = "org.contikios.cooja.mspmote.interfaces.MspMoteID"
    ET.SubElement(mid, "id").text = str(mote_id)


# ================= MOTETYPE =================

def create_motetype(parent, fw_cfg, is_rsu, start_id, count):

    mt = ET.SubElement(parent, "motetype")
    mt.text = "org.contikios.cooja.mspmote.SkyMoteType"
    
    ET.SubElement(mt, "description").text = \
    "Stationary RSU" if is_rsu else "Mobile"

    src = fw_cfg["rsu"] if is_rsu else fw_cfg["mbl"]
    binf = fw_cfg["rsu_bin"] if is_rsu else fw_cfg["mbl_bin"]

    ET.SubElement(mt, "source").text = f"[CONTIKI_DIR]/{fw_cfg['path']}/{src}"

    ET.SubElement(mt, "commands").text = \
        f"$(MAKE) -j$(CPUS) {binf} TARGET=sky"

    ET.SubElement(mt, "firmware").text = \
        f"[CONTIKI_DIR]/{fw_cfg['path']}/build/sky/{binf}"

    interfaces = [
        "org.contikios.cooja.interfaces.Position",
        "org.contikios.cooja.interfaces.IPAddress",
        "org.contikios.cooja.interfaces.Mote2MoteRelations",
        "org.contikios.cooja.interfaces.MoteAttributes",
        "org.contikios.cooja.mspmote.interfaces.MspClock",
        "org.contikios.cooja.mspmote.interfaces.MspMoteID",
        "org.contikios.cooja.mspmote.interfaces.SkyButton",
        "org.contikios.cooja.mspmote.interfaces.SkyFlash",
        "org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem",
        "org.contikios.cooja.mspmote.interfaces.Msp802154Radio",
        "org.contikios.cooja.mspmote.interfaces.MspSerial",
        "org.contikios.cooja.mspmote.interfaces.MspLED",
        "org.contikios.cooja.mspmote.interfaces.MspDebugOutput",
        "org.contikios.cooja.mspmote.interfaces.SkyTemperature"
    ]

    for iface in interfaces:
        ET.SubElement(mt, "moteinterface").text = iface

    current = start_id
    for _ in range(count):
        create_mote(mt, current)
        current += 1


# ================= PLUGINS =================

def add_visualizer(parent):

    plugin = ET.SubElement(parent, "plugin")
    plugin.text = "org.contikios.cooja.plugins.Visualizer"

    cfg = ET.SubElement(plugin, "plugin_config")
    ET.SubElement(cfg, "moterelations").text = "true"

    skins = [
        "org.contikios.cooja.plugins.skins.IDVisualizerSkin",
        "org.contikios.cooja.plugins.skins.GridVisualizerSkin",
        "org.contikios.cooja.plugins.skins.TrafficVisualizerSkin",
        "org.contikios.cooja.plugins.skins.UDGMVisualizerSkin",
        "org.contikios.cooja.plugins.skins.LEDVisualizerSkin",
        "org.contikios.cooja.plugins.skins.MoteTypeVisualizerSkin"
    ]

    for s in skins:
        ET.SubElement(cfg, "skin").text = s

    ET.SubElement(cfg, "viewport").text = \
        "1.0 0.0 0.0 1.0 10"

    ET.SubElement(plugin, "bounds",
        x="1", y="25", height="700", width="800", z="1")


def add_script(parent, location):

    plugin = ET.SubElement(parent, "plugin")
    plugin.text = "org.contikios.cooja.plugins.ScriptRunner"

    cfg = ET.SubElement(plugin, "plugin_config")

    script = (
        "GPS_LOC_REQ_Stop_Sim.js"
        if location == "Urban"
        else "HighWay_02_DistributedMobiles_01.js"
    )

    ET.SubElement(cfg, "scriptfile").text = \
        f"[COOJA_DIR]/Scripts/{script}"

    ET.SubElement(cfg, "active").text = "true"

    ET.SubElement(plugin, "bounds",
        x="1000", y="0", height="700", width="700", z="1")


def add_mobility(parent, mobiles):

    plugin = ET.SubElement(parent, "plugin")
    plugin.text = "org.contikios.cooja.plugins.Mobility"

    cfg = ET.SubElement(plugin, "plugin_config")
    ET.SubElement(cfg, "positions").text = \
        f"[CONTIKI_DIR]/examples/BGKP/mobility/urban_{mobiles}_vehicles.dat"

    ET.SubElement(plugin, "bounds",
        x="0", y="0", height="200", width="500")


# ================= CSC =================

def generate_csc(location, fw, mobiles):

    fw_cfg = FW[fw]
    total = mobiles + RSU_COUNT

    root = ET.Element("simconf", version="2023090101")
    sim = ET.SubElement(root, "simulation")

    ET.SubElement(sim, "title").text = f"{location}_{fw}_{mobiles}"
    ET.SubElement(sim, "speedlimit").text = "-1.0"
    ET.SubElement(sim, "randomseed").text = str(SEED)
    ET.SubElement(sim, "motedelay_us").text = "1000000"

    radio = ET.SubElement(sim, "radiomedium")
    radio.text = "org.contikios.cooja.radiomediums.UDGM"

    ET.SubElement(radio, "transmitting_range").text = "100.0"
    ET.SubElement(radio, "interference_range").text = "150.0"
    ET.SubElement(radio, "success_ratio_tx").text = "1.0"
    ET.SubElement(radio, "success_ratio_rx").text = "1.0"

    ev = ET.SubElement(sim, "events")
    ET.SubElement(ev, "logoutput").text = "500000"

    # MOTES
    create_motetype(sim, fw_cfg, True, 1, RSU_COUNT)
    create_motetype(sim, fw_cfg, False, RSU_COUNT + 1, mobiles)

    # CLOSE simulation FIRST ✅

    # PLUGINS OUTSIDE simulation ✅
    add_visualizer(root)
    add_script(root, location)

    if location == "Urban":
        add_mobility(root, mobiles)

    # FORMAT XML
    indent(root)

    # SAVE
    os.makedirs(SIM_DIR, exist_ok=True)

    fname = f"{location}_{fw}_{total}_20m_autoGen.csc"
    path = os.path.join(SIM_DIR, fname)

    ET.ElementTree(root).write(
        path,
        encoding="UTF-8",
        xml_declaration=True
    )

    return path


# ================= RUN =================

def run_sim(csc, location, fw, mobiles):

    print(f"[RUN] {csc}")

    subprocess.run(
        f'./gradlew run --args="--no-gui --autostart {csc}"',
        shell=True,
        cwd=COOJA,
        check=True
    )

    os.makedirs(f"{LOG_DIR}/{location}", exist_ok=True)

    dst = f"{LOG_DIR}/{location}/{location}_{fw}_{mobiles}_output.txt"

    shutil.copy(COOJA_LOG, dst)

    print(f"[LOG] {dst}")


# ================= MAIN =================

def main():

    fw = "FLUD"
    location = "Highway"
    #for location in ["Urban", "Highway"]:
        #for fw in FW:
    for mobiles in range(10, 101, 10):

                csc = generate_csc(location, fw, mobiles)
                run_sim(csc, location, fw, mobiles)
                
    print("ALL DONE")


if __name__ == "__main__":
    main()
