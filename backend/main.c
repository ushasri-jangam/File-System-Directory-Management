
/*
 FileSpace C Backend for Streamlit ADS PBL

 Data Structures:
 1. N-ary Tree - directory and file hierarchy
 2. Linked List - sibling connections
 3. Max Heap - recently accessed files
 4. Hash Table - file and directory name indexing

 Linux GCC Compilation:
 gcc main.c -o filespace_server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_NODES 500
#define HASH_SIZE 101
#define BUF_SIZE 16000

typedef int SOCKET;

#define INVALID_SOCKET -1
#define closesocket close

typedef struct Node {
    int id, is_file, starred;
    char name[80];
    struct Node *parent, *first_child, *next_sibling;
} Node;

typedef struct Entry {
    char key[80];
    Node *value;
    struct Entry *next;
} Entry;

typedef struct Recent {
    int id;
    unsigned long stamp;
} Recent;

Node *nodes[MAX_NODES];
int node_count = 0, next_id = 1;
Node *root = NULL;

Entry *table[HASH_SIZE];

Recent heap[MAX_NODES];
int heap_size = 0;
unsigned long clock_tick = 0;

/* HASH TABLE */

unsigned hash_name(const char *s) {
    unsigned h = 5381;

    while (*s)
        h = ((h << 5) + h) + (unsigned char)*s++;

    return h % HASH_SIZE;
}

void hash_add(Node *n) {
    unsigned h = hash_name(n->name);

    Entry *e = malloc(sizeof(Entry));
    if (!e) return;

    strcpy(e->key, n->name);
    e->value = n;
    e->next = table[h];
    table[h] = e;
}

void hash_remove(Node *n) {
    unsigned h = hash_name(n->name);
    Entry **p = &table[h];

    while (*p) {
        if ((*p)->value == n) {
            Entry *t = *p;
            *p = t->next;
            free(t);
            return;
        }

        p = &(*p)->next;
    }
}

Node *hash_find(const char *s) {
    for (Entry *e = table[hash_name(s)]; e; e = e->next)
        if (!strcmp(e->key, s))
            return e->value;

    return NULL;
}

/* DIRECTORY TREE */

Node *find_id(int id) {
    for (int i = 0; i < node_count; i++)
        if (nodes[i] && nodes[i]->id == id)
            return nodes[i];

    return NULL;
}

Node *make_node(const char *name, int is_file, Node *parent) {
    if (node_count >= MAX_NODES)
        return NULL;

    Node *n = calloc(1, sizeof(Node));
    if (!n) return NULL;

    n->id = next_id++;
    n->is_file = is_file;
    strncpy(n->name, name, 79);
    n->parent = parent;

    nodes[node_count++] = n;

    if (parent) {
        if (!parent->first_child)
            parent->first_child = n;
        else {
            Node *x = parent->first_child;

            while (x->next_sibling)
                x = x->next_sibling;

            x->next_sibling = n;
        }
    }

    hash_add(n);
    return n;
}

/* MAX HEAP */

void heap_push(int id) {
    if (heap_size >= MAX_NODES)
        return;

    int i = heap_size++;

    heap[i].id = id;
    heap[i].stamp = ++clock_tick;

    while (i > 0) {
        int p = (i - 1) / 2;

        if (heap[p].stamp >= heap[i].stamp)
            break;

        Recent t = heap[p];
        heap[p] = heap[i];
        heap[i] = t;

        i = p;
    }
}

/* DELETE AND MOVE OPERATIONS */

void detach(Node *n) {
    if (!n || !n->parent)
        return;

    Node **p = &n->parent->first_child;

    while (*p && *p != n)
        p = &(*p)->next_sibling;

    if (*p)
        *p = n->next_sibling;

    n->next_sibling = NULL;
}

void remove_tree(Node *n) {
    if (!n) return;

    Node *c = n->first_child;

    while (c) {
        Node *next = c->next_sibling;
        remove_tree(c);
        c = next;
    }

    hash_remove(n);

    for (int i = 0; i < node_count; i++) {
        if (nodes[i] == n) {
            nodes[i] = NULL;
            break;
        }
    }

    free(n);
}

/* JSON FUNCTIONS */

void esc(const char *s, char *out) {
    int j = 0;

    for (int i = 0; s[i] && j < 300; i++) {
        if (s[i] == '"' || s[i] == '\\')
            out[j++] = '\\';

        if ((unsigned char)s[i] >= 32)
            out[j++] = s[i];
    }

    out[j] = 0;
}

void state_json(char *out) {
    strcpy(out, "{\"nodes\":[");
    int first = 1;

    for (int i = 0; i < node_count; i++) {
        Node *n = nodes[i];

        if (!n) continue;

        char name[400], part[900];

        esc(n->name, name);

        sprintf(part,
            "%s{\"id\":%d,\"parent\":%d,\"name\":\"%s\","
            "\"type\":\"%s\",\"starred\":%s,\"children\":[",
            first ? "" : ",",
            n->id,
            n->parent ? n->parent->id : 0,
            name,
            n->is_file ? "file" : "folder",
            n->starred ? "true" : "false"
        );

        strcat(out, part);
        first = 0;

        int f = 1;

        for (Node *c = n->first_child; c; c = c->next_sibling) {
            sprintf(part, "%s%d", f ? "" : ",", c->id);
            strcat(out, part);
            f = 0;
        }

        strcat(out, "]}");
    }

    strcat(out, "],\"recent\":[");

    for (int i = 0; i < heap_size; i++) {
        char p[40];

        sprintf(p, "%s%d", i ? "," : "", heap[i].id);
        strcat(out, p);
    }

    strcat(out, "]}");
}

int jint(const char *j, const char *k, int def) {
    char p[80];

    sprintf(p, "\"%s\"", k);

    char *x = strstr((char *)j, p);

    if (!x) return def;

    x = strchr(x, ':') + 1;

    while (*x == ' ')
        x++;

    return atoi(x);
}

void jstr(const char *j, const char *k, char *out, size_t cap) {
    char p[80];

    sprintf(p, "\"%s\"", k);

    char *x = strstr((char *)j, p);

    out[0] = 0;

    if (!x) return;

    x = strchr(x, ':') + 1;

    while (*x == ' ' || *x == '"')
        x++;

    size_t i = 0;

    while (*x && *x != '"' && i + 1 < cap) {
        if (*x == '\\' && x[1])
            x++;

        out[i++] = *x++;
    }

    out[i] = 0;
}

/* BACKEND ACTIONS */

void action(const char *j) {
    char a[30], name[80], type[20];

    jstr(j, "action", a, sizeof(a));
    jstr(j, "name", name, sizeof(name));
    jstr(j, "type", type, sizeof(type));

    int id = jint(j, "id", 0);
    int pid = jint(j, "parent", 1);

    Node *n = find_id(id);

    if (!strcmp(a, "create")) {
        Node *p = find_id(pid);

        if (p && name[0])
            make_node(name, !strcmp(type, "file"), p);
    }

    else if (!strcmp(a, "rename") && n && name[0]) {
        hash_remove(n);
        strncpy(n->name, name, 79);
        n->name[79] = '\0';
        hash_add(n);
    }

    else if (!strcmp(a, "delete") && n && n != root) {
        detach(n);
        remove_tree(n);
    }

    else if (!strcmp(a, "access") && n && n->is_file) {
        heap_push(n->id);
    }

    else if (!strcmp(a, "star") && n) {
        n->starred = !n->starred;
    }

    else if (!strcmp(a, "move") && n && n != root) {
        Node *p = find_id(pid);

        if (p && p->id != n->id) {
            Node *x = p;
            int cycle = 0;

            while (x) {
                if (x == n)
                    cycle = 1;

                x = x->parent;
            }

            if (!cycle) {
                detach(n);
                n->parent = p;

                if (!p->first_child)
                    p->first_child = n;
                else {
                    Node *t = p->first_child;

                    while (t->next_sibling)
                        t = t->next_sibling;

                    t->next_sibling = n;
                }
            }
        }
    }
}

/* HTTP RESPONSE */

void respond(SOCKET s, const char *b) {
    char h[300];

    sprintf(h,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n\r\n",
        (unsigned)strlen(b)
    );

    send(s, h, strlen(h), 0);
    send(s, b, strlen(b), 0);
}

/* MAIN SERVER */

int main() {
    root = make_node("ROOT", 0, NULL);

    Node *d = make_node("Documents", 0, root);
    Node *p = make_node("Pictures", 0, root);
    Node *dl = make_node("Downloads", 0, root);

    make_node("ADS_Notes.txt", 1, d);
    make_node("Project_Report.docx", 1, d);
    make_node("Campus_Photo.jpg", 1, p);
    make_node("Dataset.zip", 1, dl);

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server);
        return 1;
    }

    if (listen(server, 8) < 0) {
        perror("listen");
        close(server);
        return 1;
    }

    printf("C ADS backend running at http://127.0.0.1:%d\n", PORT);
    printf("Keep this terminal open. Stop with Ctrl+C.\n");

    while (1) {
        SOCKET c = accept(server, NULL, NULL);

        if (c < 0)
            continue;

        char req[BUF_SIZE];

        int n = recv(c, req, sizeof(req) - 1, 0);

        if (n <= 0) {
            closesocket(c);
            continue;
        }

        req[n] = '\0';

        if (!strncmp(req, "OPTIONS", 7)) {
            respond(c, "{}");
        }

        else if (!strncmp(req, "GET /api/state", 14)) {
            char out[BUF_SIZE * 5] = {0};

            state_json(out);
            respond(c, out);
        }

        else if (!strncmp(req, "POST /api/action", 16)) {
            char *b = strstr(req, "\r\n\r\n");

            if (b)
                action(b + 4);

            char out[BUF_SIZE * 5] = {0};

            state_json(out);
            respond(c, out);
        }

        else {
            respond(c, "{}");
        }

        closesocket(c);
    }

    close(server);
    return 0;
}
