import os, sys, time
sys.path.append(os.path.join(os.environ["UE_ROOT"], "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"))
import remote_execution as re_

stmt = sys.argv[1]
r = re_.RemoteExecution()
r.start()
deadline = time.time() + 8
while time.time() < deadline and not r.remote_nodes:
    time.sleep(0.25)
if not r.remote_nodes:
    print("ERROR: no remote nodes")
    sys.exit(1)
r.open_command_connection(r.remote_nodes[0]["node_id"])
out = r.run_command(stmt, exec_mode=re_.MODE_EXEC_STATEMENT)
print("SUCCESS:", out.get("success"))
for line in out.get("output", []):
    try:
        print(f"[{line['type']}] {line['output']}")
    except UnicodeEncodeError:
        print(f"[{line['type']}] <non-ascii output>")
r.stop()
