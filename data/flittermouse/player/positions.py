import sys
f = open('positions.ini', "w")
for a in bpy.context.selected_objects:
  print('['+a.name+']', file=f)
  print('pos='+str(a.location[0])+','+str(a.location[1])+','+str(a.location[2]), file=f)
  print('rot='+str(a.rotation_euler[0])+','+str(a.rotation_euler[1])+','+str(a.rotation_euler[2]), file=f)
  print('', file=f)

f.close()
print('done')

