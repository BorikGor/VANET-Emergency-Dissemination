<?xml version='1.0' encoding='UTF-8'?>
<simconf version="2023090101">
  <simulation>
    <title>Urban_BGKP_20</title>
    <speedlimit>1.0</speedlimit>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>org.contikios.cooja.radiomediums.UDGM<transmitting_range>100.0</transmitting_range>
      <interference_range>150.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
      </radiomedium>
    <events>
      <logoutput>500000</logoutput>
      </events>
    <motetype>org.contikios.cooja.mspmote.SkyMoteType<identifier>Stationary RSU</identifier>
      <source>[CONTIKI_DIR]/examples/BGKP/rsu.c</source>
      <commands>$(MAKE) -j$(CPUS) rsu.sky TARGET=sky</commands>
      <firmware>[CONTIKI_DIR]/examples/BGKP/build/sky/rsu.sky</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyFlash</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyTemperature</moteinterface>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>1</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>2</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>3</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>4</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>5</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>6</id>
          </interface_config>
        </mote>
      </motetype>
    <motetype>org.contikios.cooja.mspmote.SkyMoteType<identifier>Mobile</identifier>
      <source>[CONTIKI_DIR]/examples/BGKP/mbl.c</source>
      <commands>$(MAKE) -j$(CPUS) mbl.sky TARGET=sky</commands>
      <firmware>[CONTIKI_DIR]/examples/BGKP/build/sky/mbl.sky</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyFlash</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyTemperature</moteinterface>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>7</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>8</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>9</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>10</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>11</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>12</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>13</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>14</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>15</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>16</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>17</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>18</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>19</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>20</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>21</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>22</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>23</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>24</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>25</id>
          </interface_config>
        </mote>
      <mote>
        <interface_config>org.contikios.cooja.interfaces.Position<pos x="0.0" y="0.0" />
          </interface_config>
        <interface_config>org.contikios.cooja.mspmote.interfaces.MspMoteID<id>26</id>
          </interface_config>
        </mote>
      </motetype>
    </simulation>
  <plugin>org.contikios.cooja.plugins.Visualizer<plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.LEDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.MoteTypeVisualizerSkin</skin>
      <viewport>1.0 0.0 0.0 1.0 10</viewport>
      </plugin_config>
    <bounds x="1" y="25" height="700" width="800" z="1" />
    </plugin>
  <plugin>org.contikios.cooja.plugins.ScriptRunner<plugin_config>
      <scriptfile>[COOJA_DIR]/Scripts/GPS_LOC_REQ_Stop_Sim.js</scriptfile>
      <active>true</active>
      </plugin_config>
    <bounds x="1000" y="0" height="700" width="700" z="1" />
    </plugin>
  <plugin>org.contikios.cooja.plugins.Mobility<plugin_config>
      <positions>[CONTIKI_DIR]/examples/BGKP/mobility/urban_20_vehicles.dat</positions>
      </plugin_config>
    <bounds x="0" y="0" height="200" width="500" />
    </plugin>
  </simconf>
