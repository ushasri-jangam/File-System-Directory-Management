import streamlit as st
import requests
from collections import defaultdict

API = "http://127.0.0.1:8080"
st.set_page_config(page_title="FileSpace | ADS PBL", page_icon="📁", layout="wide")

st.markdown("""
<style>
.stApp {background:#f7f8fc;}
.block-container {padding-top:2rem; max-width:1250px;}
h1,h2,h3 {color:#20243a;}
[data-testid="stMetric"] {background:white; border:1px solid #eceef4; padding:15px; border-radius:12px;}
div.stButton > button {border-radius:9px;}
.small-muted {color:#858da1;font-size:13px;}
</style>
""", unsafe_allow_html=True)

def api_get():
    try:
        r = requests.get(f"{API}/api/state", timeout=2)
        r.raise_for_status()
        return r.json(), None
    except Exception as e:
        return None, str(e)

def api_action(payload):
    try:
        r = requests.post(f"{API}/api/action", json=payload, timeout=3)
        r.raise_for_status()
        return r.json(), None
    except Exception as e:
        return None, str(e)

def node_map(data):
    return {n["id"]: n for n in data.get("nodes", [])}

def descendants(data, start_id):
    nm = node_map(data)
    result = []
    def walk(parent_id):
        parent = nm.get(parent_id)
        if not parent:
            return
        for child_id in parent.get("children", []):
            child = nm.get(child_id)
            if child:
                result.append(child)
                if child.get("type") == "folder":
                    walk(child_id)
    walk(start_id)
    return result

def path_for(data, node_id):
    nm = node_map(data)
    parts = []
    current = nm.get(node_id)
    while current:
        if current["id"] != 1:
            parts.append(current["name"])
        current = nm.get(current.get("parent"))
    return " / ".join(reversed(parts)) or "ROOT"

def show_tree(data, node_id=1, depth=0):
    nm = node_map(data)
    n = nm.get(node_id)
    if not n:
        return
    prefix = "　" * depth
    icon = "📁" if n["type"] == "folder" else "📄"
    st.write(f"{prefix}{icon} **{n['name']}**")
    for child_id in n.get("children", []):
        show_tree(data, child_id, depth + 1)

st.title("📁 FileSpace")
st.caption("File System Directory Management | Advanced Data Structures PBL")
data, error = api_get()

if data is None:
    st.error("C backend is not running. Start the C server first, then refresh this page.")
    st.code("cd backend\ngcc main.c -o filespace_server.exe -lws2_32\n.\\filespace_server.exe", language="powershell")
    st.info("Keep the backend terminal open. In a second terminal, run:  streamlit run app.py")
    st.stop()

nm = node_map(data)
if "current_id" not in st.session_state:
    st.session_state.current_id = 1
if st.session_state.current_id not in nm:
    st.session_state.current_id = 1

all_items = descendants(data, 1)
files = [n for n in all_items if n["type"] == "file"]
folders = [n for n in all_items if n["type"] == "folder"]
recent = [nm[i] for i in data.get("recent", []) if i in nm]

m1, m2, m3, m4 = st.columns(4)
m1.metric("Total folders", len(folders))
m2.metric("Total files", len(files))
m3.metric("Recently accessed", len(recent))
m4.metric("Hash index entries", len(all_items))

st.divider()
left, main = st.columns([1, 3], gap="large")

with left:
    st.subheader("Workspace")
    if st.button("🏠 All files", use_container_width=True):
        st.session_state.current_id = 1
        st.rerun()
    if st.button("🕘 Recent files", use_container_width=True):
        st.session_state.view = "recent"
    if st.button("⭐ Starred", use_container_width=True):
        st.session_state.view = "starred"
    st.markdown("**FOLDERS**")
    for f in [n for n in nm.values() if n.get("parent") == 1 and n["type"] == "folder"]:
        if st.button("📁 " + f["name"], key=f"nav{f['id']}", use_container_width=True):
            st.session_state.current_id = f["id"]
            st.session_state.view = "all"
            st.rerun()
    st.divider()
    st.subheader("Directory tree")
    show_tree(data)

with main:
    current = nm[st.session_state.current_id]
    view = st.session_state.get("view", "all")
    if view == "recent":
        st.subheader("🕘 Recently accessed files")
        items = recent
    elif view == "starred":
        st.subheader("⭐ Starred items")
        items = [n for n in all_items if n.get("starred")]
    else:
        st.subheader("📂 " + ("All files" if current["id"] == 1 else current["name"]))
        st.caption("Location: " + path_for(data, current["id"]))
        items = [nm[i] for i in current.get("children", []) if i in nm]

    search = st.text_input("🔎 Search by file or folder name", placeholder="Type a name to filter items")
    if search:
        items = [n for n in all_items if search.lower() in n["name"].lower()]
        st.caption(f"Search results: {len(items)}")

    st.markdown("### Manage directory")
    c1, c2 = st.columns(2)
    with c1:
        with st.expander("➕ Create folder", expanded=False):
            with st.form("create_folder"):
                folder_name = st.text_input("Folder name")
                submitted = st.form_submit_button("Create folder")
                if submitted:
                    if folder_name.strip():
                        result, err = api_action({"action":"create","parent":current["id"],"name":folder_name.strip(),"type":"folder"})
                        if err: st.error(err)
                        else: st.success("Folder created."); st.rerun()
                    else: st.warning("Enter a folder name.")
    with c2:
        with st.expander("📄 Create file", expanded=False):
            with st.form("create_file"):
                file_name = st.text_input("File name including extension")
                submitted = st.form_submit_button("Create file")
                if submitted:
                    if file_name.strip():
                        result, err = api_action({"action":"create","parent":current["id"],"name":file_name.strip(),"type":"file"})
                        if err: st.error(err)
                        else: st.success("File created."); st.rerun()
                    else: st.warning("Enter a file name.")

    st.markdown("### Items")
    if not items:
        st.info("This location is empty. Create a folder or file above.")
    for item in items:
        icon = "📁" if item["type"] == "folder" else "📄"
        with st.container(border=True):
            a, b, c = st.columns([3, 1, 1])
            with a:
                st.write(f"{icon} **{item['name']}**")
                st.caption(f"{item['type'].title()} · ID {item['id']}")
            with b:
                if item["type"] == "folder":
                    if st.button("Open", key=f"open{item['id']}"):
                        st.session_state.current_id = item["id"]
                        st.session_state.view = "all"
                        st.rerun()
                else:
                    if st.button("Access", key=f"access{item['id']}"):
                        result, err = api_action({"action":"access","id":item["id"]})
                        if err: st.error(err)
                        else: st.success("Added to recent heap."); st.rerun()
            with c:
                with st.popover("Actions"):
                    with st.form(f"rename{item['id']}"):
                        new_name = st.text_input("Rename", value=item["name"], key=f"rn{item['id']}")
                        if st.form_submit_button("Save name"):
                            result, err = api_action({"action":"rename","id":item["id"],"name":new_name.strip()})
                            if err: st.error(err)
                            else: st.success("Renamed."); st.rerun()
                    if st.button("⭐ Star / unstar", key=f"star{item['id']}"):
                        result, err = api_action({"action":"star","id":item["id"]})
                        if err: st.error(err)
                        else: st.rerun()
                    if st.button("🗑️ Delete", key=f"del{item['id']}"):
                        result, err = api_action({"action":"delete","id":item["id"]})
                        if err: st.error(err)
                        else: st.success("Deleted."); st.rerun()
                    destination_options = {f"{f['name']} (ID {f['id']})":f["id"] for f in folders if f["id"] != item["id"]}
                    if destination_options:
                        selected = st.selectbox("Move to folder", list(destination_options), key=f"moveopt{item['id']}")
                        if st.button("Move item", key=f"move{item['id']}"):
                            result, err = api_action({"action":"move","id":item["id"],"parent":destination_options[selected]})
                            if err: st.error(err)
                            else: st.success("Moved."); st.rerun()

st.divider()
st.subheader("ADS concepts used in this project")
x1, x2, x3, x4 = st.columns(4)
x1.info("**N-ary Tree**\n\nRepresents directory hierarchy.")
x2.info("**Linked List**\n\nConnects sibling files and folders.")
x3.info("**Max Heap**\n\nPrioritizes most recently accessed files.")
x4.info("**Hash Table**\n\nIndexes names for lookup.")
st.caption("Educational virtual file-system demo. It does not modify real Windows files. C backend data is currently held in memory and resets when the server restarts.")
