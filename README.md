# FileSpace — ADS PBL (B.Tech 2nd Year)

A manageable second-year Advanced Data Structures mini-project:
- Streamlit frontend (Python)
- C backend for actual ADS implementation
- N-ary tree + linked-list sibling representation
- Max heap for recently accessed files
- Hash table with separate chaining for name indexing

## Requirements
- Windows 10/11
- Python 3.10+
- VS Code
- MinGW-w64 GCC with Winsock support (`winsock2.h`, `-lws2_32`)

## 1. Open project
Extract the ZIP and open the `File_System_ADS_Streamlit` folder in VS Code.

## 2. Install Python packages
Open VS Code Terminal:
```powershell
py -m pip install -r requirements.txt
```

## 3. Compile and run C backend
Open a VS Code terminal:
```powershell
cd backend
gcc main.c -o filespace_server.exe -lws2_32
.\filespace_server.exe
```
Keep this terminal open. It listens on 127.0.0.1:8080.

## 4. Run Streamlit website
Open a SECOND VS Code terminal (leave backend terminal running):
```powershell
cd ..
py -m streamlit run app.py
```
Streamlit opens the website in your browser, usually at http://localhost:8501.

## ADS explanation
1. N-ary Tree: each folder is a node and can have multiple child nodes.
2. Linked List: `first_child` and `next_sibling` pointers link the children of a directory.
3. Max Heap: each file access receives an increasing timestamp; newest access has highest priority.
4. Hash Table: file/folder names are indexed using a hash function and separate chaining.

## Demonstration sequence
1. Create a folder named `Assignments`.
2. Open it and create `ADS_PBL.txt`.
3. Access the file to add it to the recent-files heap.
4. Rename it.
5. Star it.
6. Move it to Documents.
7. Delete a test item.
8. Show the directory tree and explain the data structures.

## Complexity discussion (expected)
- Tree traversal / full listing: O(V), where V is number of directory entries.
- Add child with sibling-chain append: O(k), where k is current sibling count.
- Hash lookup: average O(1), worst O(n) with collisions.
- Max-heap insertion: O(log n).
- Get newest item (heap root): O(1).

## Limitations to mention honestly
- This educational demo manages virtual entries, not actual Windows files.
- C backend data is in memory; restarting it resets the sample tree.
- The recent heap is a bounded demo heap; a production implementation should handle duplicate access records and heap capacity more carefully.
- Hash table is maintained in C; current Streamlit search filters the state returned by the backend. A next improvement is adding a dedicated `/api/search` endpoint that calls `hash_find()`.

## Important
Use this as a learning project. Be ready to explain each structure and function during your viva. Do not claim the app manipulates actual disk files.
