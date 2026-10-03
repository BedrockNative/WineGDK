#!/usr/bin/env python3
"""Regenerate the experimental XAML ABI from windows-rs 0.36.1 metadata.

Pass a directory containing windows-xaml{,-controls,-media}.rs, downloaded
from microsoft/windows-rs tag 0.36.1 (Windows/UI/Xaml/{,Controls/,Media/}mod.rs).
Run from the Wine source root. No network access or metadata is needed to build Wine.
"""
import argparse
from pathlib import Path
import re,uuid
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('metadata', type=Path)
args = parser.parse_args()
s='\n'.join((args.metadata / ('windows-xaml'+x+'.rs')).read_text() for x in ['', '-controls','-media'])
names=['IDependencyObject','IUIElement','IFrameworkElement','IControl','IPage','IUserControl','IContentControl','IPanel','ICanvas','ISwapChainPanel','ITextBox','ITextBox2','IButton','ISolidColorBrush','IFontFamily','IBrush']
ctypes={'bool':'boolean','f32':'FLOAT','f64':'DOUBLE','i32':'INT32','Thickness':'struct node_thickness','Point':'Point','Size':'Size','Rect':'Rect','EventRegistrationToken':'EventRegistrationToken','FontWeight':'struct node_weight','Color':'Color'}
def ctype(t):
 if t=='::windows::core::RawPtr' or t=='*mut ::core::ffi::c_void':return 'IInspectable *'
 if t=='*mut *mut ::core::ffi::c_void' or t=='*mut ::windows::core::RawPtr':return 'IInspectable **'
 if 'ManuallyDrop<' in t:return 'HSTRING *' if t.startswith('*mut ') else 'HSTRING'
 if t.startswith('*mut '):return ctype(t[5:])+' *'
 return ctypes.get(t.split('::')[-1], 'INT32')
def guid(n):
 m=re.search(r'impl ::windows::core::Interface for '+n+r' \{[^}]*?from_u128\(0x([a-f0-9_]+)',s,re.S)
 u=uuid.UUID(m[1].replace('_',''));b=u.bytes;return '{0x%08x,0x%04x,0x%04x,{%s}}'%(u.time_low,u.time_mid,u.time_hi_version,','.join('0x%02x'%v for v in b[8:]))
head=['/* XAML control ABI declarations. Derived from the Microsoft Windows SDK', ' * interface definitions published in windows-rs 0.36.1. */', 'enum node_interface { '+', '.join('NODE_'+n for n in names)+', NODE_IFACE_COUNT };']
func=[];vtables=[]
for n in names:
 body=re.search(r'pub struct '+n+r'_Vtbl \{(.*?)\n}',s,re.S)[1]
 methods=[]
 for name,args in re.findall(r'pub (\w+): unsafe extern "system" fn\((.*?)\) ->',body):
  params=[]
  for param in args.split(', ')[1:]:
   p,t=param.split(': ',1);params.append((p,ctype(t)))
  methods.append((name,params))
 head.append('static const GUID node_iid_'+n+' = '+guid(n)+';')
 head.append('struct node_'+n+'_vtbl {\n    struct node_base_vtbl base;')
 for name,params in methods:head.append('    HRESULT (WINAPI *'+name+')(struct node_iface *iface'+''.join(', '+t+' '+p for p,t in params)+');')
 head.append('};')
 methodnames={x[0] for x in methods}
 def vk(t):return 'NODE_STRING' if t=='HSTRING' else 'NODE_OBJECT' if t=='IInspectable *' else 'NODE_DATA'
 for name,params in methods:
  f='nodefn_'+n+'_'+name
  func.append('static HRESULT WINAPI '+f+'(struct node_iface *iface'+''.join(', '+t+' '+p for p,t in params)+')\n{')
  if name.startswith('Set') and name[3:] in methodnames and len(params)==1:
   pn,t=params[0];func.append('    return node_set(iface, L"'+name[3:]+'", '+vk(t)+', &'+pn+', sizeof('+pn+'));')
  elif len(params)==1 and params[0][0]=='result__':
   pn,t=params[0];under=t[:-2].strip() if t.endswith(' *') else t[:-1].strip()
   if t=='IInspectable **':under='IInspectable *'
   func.append('    return node_get(iface, L"'+name+'", '+vk(under)+', result__, sizeof(*result__));')
  elif len(params)==2 and params[0][0]=='handler' and 'EventRegistrationToken' in params[1][1]:
   func.append('    return node_event_add(iface, L"'+name+'", handler, result__);')
  elif name.startswith('Remove') and len(params)==1 and 'EventRegistrationToken' in params[0][1]:
   func.append('    return node_event_remove(iface, L"'+name[6:]+'", '+params[0][0]+');')
  elif name=='CreateCoreIndependentInputSource':func.append('    return node_create_input(iface, devicetypes, result__);')
  elif name=='Focus':func.append('    return node_focus(iface, value, result__);')
  elif name=='FindName':func.append('    return node_find_name(iface, name, result__);')
  elif name in ['Measure','Arrange','UpdateLayout','InvalidateMeasure','InvalidateArrange']:
   func.append('    return node_layout(iface);')
  elif n=='ITextBox' and name=='Select':func.append('    return node_select(iface, start, length);')
  else:
   func.append('    FIXME("'+n+'.'+name+' not implemented.\\n");\n    return E_NOTIMPL;')
  func.append('}')
 vtables.append('static const struct node_'+n+'_vtbl node_'+n+'_vtbl = {NODE_BASE, '+', '.join('nodefn_'+n+'_'+m for m,_ in methods)+'};')
head.append('static const GUID *const node_iids[] = {'+', '.join('&node_iid_'+n for n in names)+'};')
vtables.append('static const void *const node_vtables[] = {'+', '.join('&node_'+n+'_vtbl' for n in names)+'};')
Path('dlls/windows.ui.xaml/controls_abi.h').write_text('\n'.join(head)+'\n')
Path('dlls/windows.ui.xaml/controls_methods.h').write_text('\n'.join(func+vtables)+'\n')
print('Generated',len(names),'interface layouts')
