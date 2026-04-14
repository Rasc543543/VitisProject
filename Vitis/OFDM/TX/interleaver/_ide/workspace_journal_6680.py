# 2025-11-19T11:58:05.405840400
import vitis

client = vitis.create_client()
client.set_workspace(path="interleaver")

comp = client.create_hls_component(name = "interleaver_rtl",cfg_file = ["interleaver_rtl.cfg"],template = "empty_hls_component")

comp = client.get_component(name="interleaver_rtl")
comp.run(operation="C_SIMULATION")

comp.run(operation="SYNTHESIS")

vitis.dispose()

vitis.dispose()

