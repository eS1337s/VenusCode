import tkinter as tk
from tkinter import ttk
from tkinter import font

def dump(json_data):
    root = tk.Tk()
    root.title("To Venus and Beyond")
    root.geometry("700x400")
    root.configure(bg="#2a1a2f")

    style = ttk.Style(root)
    style.theme_use("clam")  

    # theme
    style.configure("Treeview",
                    background="#1f0f2e",
                    foreground="#e0dede",
                    fieldbackground="#1f0f2e",
                    font=('Segoe UI', 11))
    style.configure("Treeview.Heading",
                    background="#3e224a",
                    foreground="#d1c7e0",
                    font=('Segoe UI Semibold', 12))
    style.map("Treeview",
              background=[('selected', '#5a3d7a')],
              foreground=[('selected', '#ffffff')])

    # framepadding
    frame = tk.Frame(root, bg="#2a1a2f", padx=10, pady=10)
    frame.pack(fill='both', expand=True)

    columns = ("robot", "id", "colour", "size", "coordinates")

    tree = ttk.Treeview(frame, columns=columns, show='headings', selectmode='browse')
    tree.pack(fill='both', expand=True)

    #headings
    tree.heading("robot", text="Robot")
    tree.heading("id", text="ID")
    tree.heading("colour", text="Colour")
    tree.heading("size", text="Size")
    tree.heading("coordinates", text="Coordinates")

    #column widths
    tree.column("robot", width=60, anchor='center')
    tree.column("id", width=50, anchor='center')
    tree.column("colour", width=100, anchor='center')
    tree.column("size", width=60, anchor='center')
    tree.column("coordinates", width=150, anchor='center')

    #data
    for item in json_data:
        coords = f"({item['coordinates'][0]}, {item['coordinates'][1]})"
        tree.insert("", "end", values=(
            item['robot'],
            item['id'],
            item['colour'],
            str(item['size']),
            coords
        ))

    #vertical scrollbar
    vsb = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
    vsb.pack(side='right', fill='y')
    tree.configure(yscrollcommand=vsb.set)

    #horizontal scrollbar
    hsb = ttk.Scrollbar(root, orient="horizontal", command=tree.xview)
    hsb.pack(side='bottom', fill='x')
    tree.configure(xscrollcommand=hsb.set)

    root.mainloop()
