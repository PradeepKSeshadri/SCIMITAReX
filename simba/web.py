#!/usr/bin/python
import os
import glob
import fnmatch
import sqlite3
import argparse
from flask import Flask, request, render_template, send_file, redirect

print("==================================")
print("SIMBA: SIMulation Browser Analysis")
print("==================================")

parser = argparse.ArgumentParser(description='Start a webserver to brows database entries');
parser.add_argument('-i','--ip', default='127.0.0.1', help='IP address of server (default: localhost)');
parser.add_argument('-p','--port', default='5000', help='Port (default: 5000)');
parser.add_argument('-d','--database',default='results.db',help='Name of database to read from')
args=parser.parse_args();

script_directory = os.path.realpath(__file__)

app = Flask(__name__)


@app.route("/")
def root():
    db = sqlite3.connect(args.database)
    db.text_factory = str
    cur= db.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = [r[0] for r in cur.fetchall()]
    if len(tables) > 0:
        return redirect('/table/'+tables[0])
    return render_template('root.html',
                           tables=tables)

@app.route("/table/<table>")
def table(table):
    db = sqlite3.connect(args.database)
    db.text_factory = str
    cur= db.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = [r[0] for r in cur.fetchall()]

    if not table: table_name = tables[0]
    else: table_name = table

    cur.execute("SELECT * FROM " + table_name )
    data = cur.fetchall()

    cur.execute("PRAGMA table_info("+tables[0]+")")
    columns=[a[1] for a in cur.fetchall()]

    db.commit()
    db.close()
    
    if table==None or table not in tables: table = tables[0];

    return render_template('template.html',
                           tables=tables,
                           table_name=table,
                           table=data,
                           columns=columns)
imgfiles = []

def find_images(path):
    global imgfiles
    img_fmts = ['.jpg', '.jpeg', '.png', '.pdf']
    imgfiles = []
    for fmt in img_fmts: imgfiles += glob.glob(path+'/*'+fmt)
    imgfiles.sort()

@app.route('/img/<number>')
def serve_image(number):
    global imgfiles
    return send_file(imgfiles[int(number)])

@app.route('/table/<table>/entry/<entry>', methods=['GET','POST'])
def table_entry(table,entry):
    global imgfiles

    db = sqlite3.connect('results.db')
    db.text_factory = str
    cur= db.cursor()
    
    if request.method == 'POST':
        if request.form.get('description'):
            cur.execute("UPDATE " + table + " SET Description = ? WHERE HASH = ?;",
                        (request.form.get('description'), entry));
        if request.form.get('tags'):
            cur.execute("UPDATE " + table + " SET Tags = ? WHERE HASH = ?;",
                        (request.form.get('tags'), entry));

    cur.execute("PRAGMA table_info("+table+")")
    columns=[a[1] for a in cur.fetchall()]

    cur.execute("SELECT * FROM " + table + " WHERE HASH='" + entry + "'")
    data = cur.fetchall()[0]

    find_images(data[1])

    db.commit()
    db.close()
    
    return render_template('detail.html',
                           table=table,
                           entry=entry,
                           columns=columns,
                           data=data,
                           imgfiles=[os.path.split(im)[1] for im in imgfiles])

if __name__ == '__main__':
    app.run(debug=True,
            use_reloader=False,
            host=args.ip,
            port=int(args.port))